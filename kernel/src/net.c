#include "net.h"
#include "io.h"
#include "libk.h"

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC
#define RTL_VENDOR_DEVICE 0x813910ECu
#define RTL_RX_BUF 8192
#define RTL_TX_BUF 1792

#define RTL_IDR0 0x00
#define RTL_TSD0 0x10
#define RTL_TSAD0 0x20
#define RTL_RBSTART 0x30
#define RTL_CMD 0x37
#define RTL_CAPR 0x38
#define RTL_IMR 0x3C
#define RTL_ISR 0x3E
#define RTL_RCR 0x44

#define ETH_ARP 0x0806
#define ETH_IPV4 0x0800
#define IP_ICMP 1

static uint16_t rtl_base;
static uint8_t rtl_found;
static uint8_t rx_buffer[RTL_RX_BUF + 16] __attribute__((aligned(16)));
static uint8_t tx_buffer[4][RTL_TX_BUF] __attribute__((aligned(4)));
static uint16_t rx_offset;
static uint8_t tx_index;
static const uint8_t local_ip[4] = {10,0,2,15};
static const uint8_t gateway_ip[4] = {10,0,2,2};
static uint8_t local_mac[6];
static uint8_t gateway_mac[6];
static uint8_t gateway_known;
static uint8_t ping_reply;
static uint16_t ping_sequence;
static uint8_t ping_target[4];
static uint8_t ping_pending;

static void copy_mac(uint8_t *out,const uint8_t *in){for(uint8_t i=0;i<6;i++)out[i]=in[i];}
static uint8_t ip_equal(const uint8_t *a,const uint8_t *b){for(uint8_t i=0;i<4;i++)if(a[i]!=b[i])return 0;return 1;}
static uint16_t checksum(const uint8_t *data,uint16_t length) { uint32_t sum=0; while(length>1){sum+=((uint16_t)data[0]<<8)|data[1];data+=2;length-=2;}if(length)sum+=(uint16_t)data[0]<<8;while(sum>>16)sum=(sum&0xFFFF)+(sum>>16);return (uint16_t)~sum; }

static uint32_t pci_read(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset) {
    outl(PCI_ADDR,0x80000000u|((uint32_t)bus<<16)|((uint32_t)device<<11)|((uint32_t)function<<8)|(offset&0xFC));
    return inl(PCI_DATA);
}
static void pci_write(uint8_t bus,uint8_t device,uint8_t function,uint8_t offset,uint32_t value) {
    outl(PCI_ADDR,0x80000000u|((uint32_t)bus<<16)|((uint32_t)device<<11)|((uint32_t)function<<8)|(offset&0xFC)); outl(PCI_DATA,value);
}

static void rtl_send(const uint8_t *frame,uint16_t length) {
    if(!rtl_found||length>RTL_TX_BUF)return;
    uint8_t slot=tx_index++&3; kmemcpy(tx_buffer[slot],frame,length);
    outl(rtl_base+RTL_TSAD0+slot*4,(uint32_t)tx_buffer[slot]);
    outl(rtl_base+RTL_TSD0+slot*4,length);
}

static void send_arp_request(void) {
    uint8_t frame[42];
    for(uint8_t i=0;i<6;i++)frame[i]=0xFF;
    copy_mac(frame+6,local_mac); frame[12]=0x08;frame[13]=0x06;
    frame[14]=0;frame[15]=1;frame[16]=0x08;frame[17]=0;frame[18]=6;frame[19]=4;frame[20]=0;frame[21]=1;
    copy_mac(frame+22,local_mac);kmemcpy(frame+28,local_ip,4);kmemset(frame+32,0,6);kmemcpy(frame+38,gateway_ip,4);rtl_send(frame,sizeof(frame));
}

static void send_ping(void) {
    if(!gateway_known)return;
    uint8_t frame[42]; copy_mac(frame,gateway_mac);copy_mac(frame+6,local_mac);frame[12]=0x08;frame[13]=0;
    uint8_t *ip=frame+14;ip[0]=0x45;ip[1]=0;ip[2]=0;ip[3]=28;ip[4]=0;ip[5]=1;ip[6]=0;ip[7]=0;ip[8]=64;ip[9]=IP_ICMP;ip[10]=ip[11]=0;kmemcpy(ip+12,local_ip,4);kmemcpy(ip+16,ping_target,4);uint16_t sum=checksum(ip,20);ip[10]=(uint8_t)(sum>>8);ip[11]=(uint8_t)sum;
    uint8_t *icmp=ip+20;icmp[0]=8;icmp[1]=0;icmp[2]=icmp[3]=0;icmp[4]=0x4D;icmp[5]=0x4F;icmp[6]=(uint8_t)(ping_sequence>>8);icmp[7]=(uint8_t)ping_sequence++;sum=checksum(icmp,8);icmp[2]=(uint8_t)(sum>>8);icmp[3]=(uint8_t)sum;rtl_send(frame,sizeof(frame));
}

static void handle_frame(const uint8_t *frame,uint16_t length) {
    if(length<14)return; uint16_t type=((uint16_t)frame[12]<<8)|frame[13];
    if(type==ETH_ARP&&length>=42&&frame[20]==0&&frame[21]==2&&ip_equal(frame+28,gateway_ip)){copy_mac(gateway_mac,frame+22);gateway_known=1;if(ping_pending){ping_pending=0;send_ping();}return;}
    if(type==ETH_IPV4&&length>=42&&frame[23]==IP_ICMP&&ip_equal(frame+30,local_ip)){const uint8_t *icmp=frame+34;if(icmp[0]==0&&icmp[4]==0x4D&&icmp[5]==0x4F)ping_reply=1;}
}

void net_init(void) {
    for(uint16_t bus=0;bus<256&&!rtl_found;bus++)for(uint8_t dev=0;dev<32;dev++)if(pci_read((uint8_t)bus,dev,0,0)==RTL_VENDOR_DEVICE){uint32_t command=pci_read((uint8_t)bus,dev,0,4);pci_write((uint8_t)bus,dev,0,4,command|7);uint32_t bar=pci_read((uint8_t)bus,dev,0,0x10);rtl_base=(uint16_t)(bar&0xFFFC);rtl_found=rtl_base!=0;}
    if(!rtl_found)return;
    outb(rtl_base+RTL_CMD,0x10);for(uint32_t i=0;i<100000;i++)if(!(inb(rtl_base+RTL_CMD)&0x10))break;
    for(uint8_t i=0;i<6;i++)local_mac[i]=inb(rtl_base+RTL_IDR0+i);
    outl(rtl_base+RTL_RBSTART,(uint32_t)rx_buffer);outw(rtl_base+RTL_CAPR,0);outl(rtl_base+RTL_RCR,0x0000F);outw(rtl_base+RTL_IMR,0);outb(rtl_base+RTL_CMD,0x0C);rx_offset=0;send_arp_request();
}

void net_poll(void) {
    if(!rtl_found)return;
    while((inb(rtl_base+RTL_CMD)&1)==0){uint16_t status=*(uint16_t*)(rx_buffer+rx_offset);uint16_t length=*(uint16_t*)(rx_buffer+rx_offset+2);if((status&1)&&length>=4&&length<=RTL_RX_BUF)handle_frame(rx_buffer+rx_offset+4,(uint16_t)(length-4));uint16_t next=(uint16_t)((rx_offset+length+4+3)&~3);if(next>=RTL_RX_BUF)next-=RTL_RX_BUF;rx_offset=next;outw(rtl_base+RTL_CAPR,(uint16_t)(rx_offset-16));}
}

static uint8_t parse_ip(const char *text,uint8_t *ip) {
    for(uint8_t part=0;part<4;part++) { uint16_t value=0;uint8_t digits=0;while(*text>='0'&&*text<='9'){value=(uint16_t)(value*10+(*text++-'0'));digits++;if(value>255)return 0;}if(!digits)return 0;ip[part]=(uint8_t)value;if(part<3){if(*text++!='.')return 0;}else if(*text)return 0; } return 1;
}
void net_ping_gateway(void) { kmemcpy(ping_target,gateway_ip,4);ping_reply=0;ping_pending=1;if(!rtl_found)return;if(gateway_known){ping_pending=0;send_ping();}else send_arp_request(); }
uint8_t net_ping_ip(const char *address) { if(!parse_ip(address,ping_target))return 0; ping_reply=0;ping_pending=1;if(!rtl_found)return 1;if(gateway_known){ping_pending=0;send_ping();}else send_arp_request();return 1; }
uint8_t net_ready(void){return rtl_found;}
uint8_t net_gateway_known(void){return gateway_known;}
uint8_t net_ping_ok(void){return ping_reply;}
