#include "idt.h"
struct IDTR _idt_idtr;
static struct InterruptDescriptorTable _idt_table __attribute__((aligned(0x1000)));
void set_interrupt_gate(uint8_t int_vector, void *handler_address, uint16_t gdt_seg_selector, uint8_t privilege){
    struct IDTGate *entry=&_idt_idtr.address->table[int_vector];
    entry->offset_low=0xffff&(uint64_t)handler_address;
    entry->offset_mid= ((uint64_t)handler_address >>16) &0xFFFF;
    entry->offset_high= ((uint64_t)handler_address >>32) &0xFFFFFFFF;
    entry->selector=gdt_seg_selector;
    entry->ist=0; // for now
    entry->type_attributes=0x8E | ((privilege & 0x3) << 5);
}
void initialize_idt(void){
    _idt_idtr.size = (uint16_t)(sizeof(struct IDTGate) * IDT_MAX_ENTRY_COUNT - 1);
    _idt_idtr.address = &_idt_table;
    for (int i = 0; i < ISR_STUB_TABLE_LIMIT; i++)
    {
        if(i==0x30){
           set_interrupt_gate(i,isr_stub_table[i],0x08,3); 
        } else {
            set_interrupt_gate(i,isr_stub_table[i],0x08,0);
        }
    }

    __asm__ volatile ("lidt %0" : : "m" (_idt_idtr));
    __asm__ volatile("sti");
}
