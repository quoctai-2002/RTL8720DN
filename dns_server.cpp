#include "dns_server.h"

static DNSServer* dnsServerInstance = NULL;
DNSServer dnsServer;  // Global instance

DNSServer::DNSServer() {
    // Set default resolved IP to 192.168.1.1
    _resolvedIP[0] = 192;
    _resolvedIP[1] = 168;
    _resolvedIP[2] = 1;
    _resolvedIP[3] = 1;
    _dns_server_pcb = NULL;
}

void DNSServer::setResolvedIP(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3) {
    _resolvedIP[0] = ip0;
    _resolvedIP[1] = ip1;
    _resolvedIP[2] = ip2;
    _resolvedIP[3] = ip3;
}

bool DNSServer::requestIncludesOnlyOneQuestion(DNSHeader &dnsHeader) {
    return ntohs(dnsHeader.QDCount) == 1 && dnsHeader.ANCount == 0 && dnsHeader.NSCount == 0 && dnsHeader.ARCount == 0;
}

void DNSServer::begin() {
    dnsServerInstance = this;
    
    // Clean up any existing DNS server PCB
    struct udp_pcb *pcb;
    for (pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
        if (pcb->local_port == DNS_SERVER_PORT) {
            DEBUG_SER_PRINT("Removing existing DNS PCB\n");
            udp_remove(pcb);
            break;  // Only remove one
        }
    }

    // Create a new DNS service
    _dns_server_pcb = udp_new();
    DEBUG_SER_PRINT("Created DNS PCB\n");
    udp_bind(_dns_server_pcb, IP4_ADDR_ANY, DNS_SERVER_PORT);
    udp_recv(_dns_server_pcb, (udp_recv_fn)packetHandler, NULL);
    DEBUG_SER_PRINT("DNS Server bound to port 53\n");
}

void DNSServer::stop() {
    if (_dns_server_pcb) {
        udp_remove(_dns_server_pcb);
        _dns_server_pcb = NULL;
        dnsServerInstance = NULL;
        DEBUG_SER_PRINT("DNS Server stopped\n");
    }
}

void DNSServer::packetHandler(void *arg, struct udp_pcb *udp_pcb, struct pbuf *udp_packet_buffer, struct ip_addr *sender_addr, uint16_t sender_port) {
    (void)arg;
    
    // Use the static instance to access non-static members
    if (!dnsServerInstance || !udp_packet_buffer || udp_packet_buffer->len < DNS_HEADER_SIZE) {
        if (udp_packet_buffer) pbuf_free(udp_packet_buffer);
        return;
    }

    DEBUG_SER_PRINT("DNS query received\n");

    DNSHeader dnsHeader;
    DNSQuestion dnsQuestion;
    
    // Copy DNS header from packet
    memcpy(&dnsHeader, udp_packet_buffer->payload, DNS_HEADER_SIZE);

    if (dnsServerInstance->requestIncludesOnlyOneQuestion(dnsHeader)) {
        // Validate there's enough data for at least one byte past header
        if (udp_packet_buffer->len <= DNS_HEADER_SIZE) {
            pbuf_free(udp_packet_buffer);
            return;
        }
        
        // Find end of query name (safely)
        uint16_t offset = DNS_HEADER_SIZE;
        uint16_t nameLength = 0;
        while (offset < udp_packet_buffer->len && ((uint8_t*)udp_packet_buffer->payload)[offset] != 0) {
            nameLength++;
            offset++;
        }
        
        // Check if we found a valid end and have enough space for type and class
        if (offset >= udp_packet_buffer->len - 4) {
            pbuf_free(udp_packet_buffer);
            return;
        }
        
        // Move past the terminating zero
        offset++;
        nameLength++;
        
        dnsQuestion.QName = (uint8_t *)udp_packet_buffer->payload + DNS_HEADER_SIZE;
        dnsQuestion.QNameLength = nameLength;
        int sizeUrl = static_cast<int>(nameLength);

        struct dns_hdr *hdr = (struct dns_hdr *)udp_packet_buffer->payload;
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct dns_hdr) + sizeUrl + 20, PBUF_RAM);

        if (p) {
            // Prepare DNS response
            struct dns_hdr *rsp_hdr = (struct dns_hdr *)p->payload;
            rsp_hdr->id = hdr->id;
            rsp_hdr->flags1 = 0x85;  // QR=1, AA=1, RD=1
            rsp_hdr->flags2 = 0x80;  // RA=1
            rsp_hdr->numquestions = PP_HTONS(1);
            rsp_hdr->numanswers = PP_HTONS(1);
            rsp_hdr->numauthrr = PP_HTONS(0);
            rsp_hdr->numextrarr = PP_HTONS(0);

            // Copy query and set response fields
            uint8_t *responsePtr = (uint8_t *)rsp_hdr + sizeof(struct dns_hdr);
            memcpy(responsePtr, dnsQuestion.QName, sizeUrl);
            responsePtr += sizeUrl;
            
            // Set DNS response record fields
            *(uint16_t *)responsePtr = PP_HTONS(1);      // Type A
            *(uint16_t *)(responsePtr + 2) = PP_HTONS(1); // Class IN
            responsePtr[4] = 0xc0;                        // Compressed name pointer
            responsePtr[5] = 0x0c;                        // Points to question name
            *(uint16_t *)(responsePtr + 6) = PP_HTONS(1); // Type A
            *(uint16_t *)(responsePtr + 8) = PP_HTONS(1); // Class IN
            *(uint32_t *)(responsePtr + 10) = PP_HTONL(60); // TTL: 60 seconds
            *(uint16_t *)(responsePtr + 14) = PP_HTONS(4); // Data length
            memcpy(responsePtr + 16, dnsServerInstance->_resolvedIP, 4);

            // Send response
            udp_sendto(udp_pcb, p, sender_addr, sender_port);
            pbuf_free(p);
            DEBUG_SER_PRINT("DNS response sent -> 192.168.1.1\n");
        }
    } else {
        // Create a new buffer for response to avoid modifying the original
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, udp_packet_buffer->len, PBUF_RAM);
        if (p) {
            // Copy the original packet
            memcpy(p->payload, udp_packet_buffer->payload, udp_packet_buffer->len);
            
            // Flags for error response
            struct dns_hdr *dns_rsp = (struct dns_hdr *)p->payload;
            dns_rsp->flags1 |= 0x80;  // Response flag
            dns_rsp->flags2 = 0x05;   // Query Refused
            
            udp_sendto(udp_pcb, p, sender_addr, sender_port);
            pbuf_free(p);
        }
    }

    // Cleanup
    pbuf_free(udp_packet_buffer);
}

// Wrapper functions for Evil Twin integration
void startDNSServer() {
    dnsServer.setResolvedIP(192, 168, 1, 1);
    dnsServer.begin();
}

void stopDNSServer() {
    dnsServer.stop();
}

// Empty - lwIP handles DNS via callbacks
void processDNS() {
    // No action needed - lwIP udp_recv callback handles everything
}

// Empty task - DNS is handled by lwIP callbacks
void DNSServerTask(void *pvParameters) {
    (void) pvParameters;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // Sleep forever
    }
}
