#include "keyboard.h"
#include "io.h"

#define KEYBOARD_DATA 0x60
#define KEYBOARD_STATUS 0x64
#define KEYBOARD_BUFFER_SIZE 64

static volatile uint16_t events[KEYBOARD_BUFFER_SIZE];
static volatile uint8_t read_index;
static volatile uint8_t write_index;
static uint8_t shift_pressed;
static uint8_t caps_lock;
static uint8_t extended_prefix;

static char translate(uint8_t scan) {
    static const char normal[128] = {
        [2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',
        [12]='-',[13]='=',[26]='[',[27]=']',[39]=';',[40]='\'',[41]='`',[43]='\\',[51]=',',[52]='.',[53]='/',
        [16]='q',[17]='w',[18]='e',[19]='r',[20]='t',[21]='y',[22]='u',[23]='i',[24]='o',[25]='p',
        [30]='a',[31]='s',[32]='d',[33]='f',[34]='g',[35]='h',[36]='j',[37]='k',[38]='l',
        [44]='z',[45]='x',[46]='c',[47]='v',[48]='b',[49]='n',[50]='m',[57]=' '
    };
    static const char shifted[128] = {
        [2]='!',[3]='@',[4]='#',[5]='$',[6]='%',[7]='^',[8]='&',[9]='*',[10]='(',[11]=')',
        [12]='_',[13]='+',[26]='{',[27]='}',[39]=':',[40]='"',[41]='~',[43]='|',[51]='<',[52]='>',[53]='?',
        [16]='Q',[17]='W',[18]='E',[19]='R',[20]='T',[21]='Y',[22]='U',[23]='I',[24]='O',[25]='P',
        [30]='A',[31]='S',[32]='D',[33]='F',[34]='G',[35]='H',[36]='J',[37]='K',[38]='L',
        [44]='Z',[45]='X',[46]='C',[47]='V',[48]='B',[49]='N',[50]='M',[57]=' '
    };
    char character = normal[scan];
    if (character >= 'a' && character <= 'z') return (shift_pressed ^ caps_lock) ? shifted[scan] : character;
    return shift_pressed ? shifted[scan] : character;
}

static void push_event(uint16_t event) {
    uint8_t next = (uint8_t)((write_index + 1) % KEYBOARD_BUFFER_SIZE);
    if (next == read_index) return;
    events[write_index] = event;
    write_index = next;
}

void keyboard_init(void) {
    read_index = write_index = 0;
    shift_pressed = caps_lock = extended_prefix = 0;
    while (inb(KEYBOARD_STATUS) & 1) (void)inb(KEYBOARD_DATA);
}

void keyboard_irq_handler(void) {
    uint8_t scan = inb(KEYBOARD_DATA);
    if (scan == 0xE0) { extended_prefix = 1; return; }
    uint8_t released = scan & 0x80;
    scan &= 0x7F;
    if (scan == 42 || scan == 54) { shift_pressed = !released; extended_prefix = 0; return; }
    if (scan == 58 && !released) { caps_lock ^= 1; extended_prefix = 0; return; }
    if (released) { extended_prefix = 0; return; }

    if (extended_prefix) {
        uint16_t key = KEY_NONE;
        if (scan == 75) key = KEY_LEFT;
        else if (scan == 77) key = KEY_RIGHT;
        else if (scan == 72) key = KEY_UP;
        else if (scan == 80) key = KEY_DOWN;
        else if (scan == 71) key = KEY_HOME;
        else if (scan == 79) key = KEY_END;
        else if (scan == 83) key = KEY_DELETE;
        if (key) push_event(key);
        extended_prefix = 0;
        return;
    }
    if (scan == 14) push_event('\b');
    else if (scan == 28) push_event('\n');
    else {
        char character = translate(scan);
        if (character) push_event((uint8_t)character);
    }
}

uint16_t keyboard_pop_event(void) {
    if (read_index == write_index) return KEY_NONE;
    uint16_t event = events[read_index];
    read_index = (uint8_t)((read_index + 1) % KEYBOARD_BUFFER_SIZE);
    return event;
}
