// gbclock - reloj flip-clock estilo Fliqlo para Game Boy (GBDK-2020)
//
// Pantalla: 4 tarjetas de 32x64 px (HH:MM). Cada tarjeta usa 32 tiles de fondo
// que se reescriben en tiempo real para animar el giro (la mitad de arriba se
// pliega hacia abajo y despues se despliega la mitad de abajo del digito nuevo).
//
// Controles:
//   Poner hora : arriba/abajo cambia, A confirma (hora -> minutos -> corre)
//   Corriendo  : START vuelve a poner la hora, B suma 1 minuto (para probar el flip)

#include <gb/gb.h>
#include <stdint.h>
#include <string.h>

extern const uint8_t digit_cards[10][512];

// ---- Timing ----
// La Game Boy hace 59.7275 frames/seg. Acumulamos 100 por frame y restamos 5973
// cada segundo, asi el reloj no se adelanta como con "60 ticks".
#define TICK_ADD 100
#define TICK_SEC 5973

static volatile uint8_t sec_pending = 0;
static uint16_t acc = 0;

static void vbl_isr(void) {
    acc += TICK_ADD;
    if (acc >= TICK_SEC) {
        acc -= TICK_SEC;
        sec_pending++;
    }
}

// ---- Layout ----
#define CARD_ROW   5      // fila de tiles donde empiezan las tarjetas (5..12)
#define CARD_TILES 32     // 4 x 8 tiles por tarjeta
#define COLON_TILE 129    // 4 tiles: izq/der arriba, izq/der abajo
#define SLOT_BASE(s) (1 + (s) * CARD_TILES)

static const uint8_t slot_col[4] = {1, 5, 11, 15};

static uint8_t buf[512];
static uint8_t shown[4]  = {0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t target[4];
static uint8_t anim_step[4];
static uint8_t anim_old[4];
static uint8_t anim_new[4];

// ---- Estado del reloj ----
typedef enum { STATE_SET_HOUR, STATE_SET_MIN, STATE_RUNNING } state_t;
static state_t state = STATE_SET_HOUR;
static uint8_t seconds = 0, minutes = 0, hours = 12;
static uint8_t colon_on = 1;
static uint8_t frame = 0;

// ---- Render de tarjetas ----

// Copia una fila de 64 (8 bytes: 4 tiles x 2 bytes) al buffer en formato tile-major.
static void put_row(uint8_t r, const uint8_t *src) {
    uint8_t *d = buf + ((uint16_t)(r >> 3) * 64) + ((r & 7) << 1);
    uint8_t tx;
    for (tx = 0; tx < 4; tx++) {
        d[tx * 16]     = src[tx * 2];
        d[tx * 16 + 1] = src[tx * 2 + 1];
    }
}

static const uint8_t *card_row(uint8_t digit, uint8_t r) {
    return digit_cards[digit] + ((uint16_t)r << 3);
}

static void upload(uint8_t slot) {
    set_bkg_data(SLOT_BASE(slot), CARD_TILES, buf);
}

static void render_static(uint8_t slot, uint8_t digit) {
    uint8_t r;
    for (r = 0; r < 64; r++) put_row(r, card_row(digit, r));
    upload(slot);
}

// Un cuadro de la animacion de giro. step = 1..6
static void render_flip(uint8_t slot, uint8_t from, uint8_t to, uint8_t step) {
    uint8_t r, h, i;

    // mitad de arriba (filas 0..31)
    if (step <= 3) {
        h = 32 - step * 8;                 // 24, 16, 8: el digito viejo se aplasta
        for (r = 0; r < 32 - h; r++) put_row(r, card_row(to, r));
        for (i = 0; i < h; i++)
            put_row(32 - h + i, card_row(from, (uint8_t)((uint16_t)i * 32 / h)));
    } else {
        for (r = 0; r < 32; r++) put_row(r, card_row(to, r));
    }

    // mitad de abajo (filas 32..63)
    if (step <= 3) {
        for (r = 32; r < 64; r++) put_row(r, card_row(from, r));
    } else {
        h = (step - 3) * 8;                // 8, 16, 24: el digito nuevo se despliega
        for (i = 0; i < h; i++)
            put_row(32 + i, card_row(to, 32 + (uint8_t)((uint16_t)i * 32 / h)));
        for (r = 32 + h; r < 64; r++) put_row(r, card_row(from, r));
    }
    upload(slot);
}

static void show_card(uint8_t slot, uint8_t visible) {
    uint8_t m[CARD_TILES], i;
    for (i = 0; i < CARD_TILES; i++) m[i] = visible ? (SLOT_BASE(slot) + i) : 0;
    set_bkg_tiles(slot_col[slot], CARD_ROW, 4, 8, m);
}

static void draw_colon(uint8_t on) {
    uint8_t m[4];
    m[0] = on ? COLON_TILE     : 0;
    m[1] = on ? COLON_TILE + 1 : 0;
    m[2] = on ? COLON_TILE + 2 : 0;
    m[3] = on ? COLON_TILE + 3 : 0;
    set_bkg_tiles(9, CARD_ROW + 3, 2, 2, m);
}

static void make_dot_tile(uint8_t *t, uint8_t val, uint8_t r0, uint8_t r1) {
    uint8_t r;
    for (r = 0; r < 8; r++) {
        uint8_t v = (r >= r0 && r <= r1) ? val : 0xFF;
        t[r * 2] = v;
        t[r * 2 + 1] = v;
    }
}

// ---- Logica ----

static void compute_targets(void) {
    target[0] = hours / 10;
    target[1] = hours % 10;
    target[2] = minutes / 10;
    target[3] = minutes % 10;
}

static void add_minute(void) {
    minutes++;
    if (minutes >= 60) {
        minutes = 0;
        hours++;
        if (hours >= 24) hours = 0;
    }
}

static void tick_second(void) {
    colon_on = !colon_on;
    draw_colon(colon_on);
    seconds++;
    if (seconds >= 60) {
        seconds = 0;
        add_minute();
    }
}

static void enter_state(state_t s) {
    uint8_t i;
    state = s;
    for (i = 0; i < 4; i++) show_card(i, 1);
    colon_on = 1;
    draw_colon(1);
}

static void handle_input(uint8_t pressed) {
    if (state == STATE_SET_HOUR) {
        if (pressed & J_UP)   hours = (hours + 1) % 24;
        if (pressed & J_DOWN) hours = (hours == 0) ? 23 : hours - 1;
        if (pressed & J_A)    enter_state(STATE_SET_MIN);
    } else if (state == STATE_SET_MIN) {
        if (pressed & J_UP)   minutes = (minutes + 1) % 60;
        if (pressed & J_DOWN) minutes = (minutes == 0) ? 59 : minutes - 1;
        if (pressed & J_A) {
            seconds = 0;
            acc = 0;
            sec_pending = 0;
            enter_state(STATE_RUNNING);
        }
    } else {
        if (pressed & J_START) enter_state(STATE_SET_HOUR);
        if (pressed & J_B)     add_minute();
    }
}

void main(void) {
    uint8_t i, joy, last_joy = 0, pressed;
    uint8_t t[16];

    DISPLAY_OFF;
    BGP_REG = 0xE4;

    // tile 0 = negro (fondo)
    memset(t, 0xFF, 16);
    set_bkg_data(0, 1, t);

    // puntos del colon (blancos sobre negro)
    make_dot_tile(t, 0xFC, 3, 6); set_bkg_data((uint8_t)(COLON_TILE + 0), 1, t);
    make_dot_tile(t, 0x3F, 3, 6); set_bkg_data((uint8_t)(COLON_TILE + 1), 1, t);
    make_dot_tile(t, 0xFC, 1, 4); set_bkg_data((uint8_t)(COLON_TILE + 2), 1, t);
    make_dot_tile(t, 0x3F, 1, 4); set_bkg_data((uint8_t)(COLON_TILE + 3), 1, t);

    fill_bkg_rect(0, 0, 20, 18, 0);

    compute_targets();
    for (i = 0; i < 4; i++) {
        render_static(i, target[i]);
        shown[i] = target[i];
        show_card(i, 1);
    }
    draw_colon(1);

    SHOW_BKG;
    DISPLAY_ON;
    add_VBL(vbl_isr);

    while (1) {
        wait_vbl_done();
        frame++;

        joy = joypad();
        pressed = joy & ~last_joy;
        last_joy = joy;
        handle_input(pressed);

        // segundos reales (solo cuentan corriendo)
        while (sec_pending) {
            disable_interrupts();
            sec_pending--;
            enable_interrupts();
            if (state == STATE_RUNNING) tick_second();
        }

        compute_targets();

        if (state == STATE_RUNNING) {
            for (i = 0; i < 4; i++) {
                if (anim_step[i] == 0 && target[i] != shown[i]) {
                    anim_old[i] = shown[i];
                    anim_new[i] = target[i];
                    anim_step[i] = 1;
                }
                if (anim_step[i] && (frame & 1) == 0) {
                    render_flip(i, anim_old[i], anim_new[i], anim_step[i]);
                    anim_step[i]++;
                    if (anim_step[i] > 6) {
                        render_static(i, anim_new[i]);
                        shown[i] = anim_new[i];
                        anim_step[i] = 0;
                    }
                }
            }
        } else {
            // poniendo la hora: sin animacion, y parpadea el par que se edita
            for (i = 0; i < 4; i++) {
                anim_step[i] = 0;
                if (target[i] != shown[i]) {
                    render_static(i, target[i]);
                    shown[i] = target[i];
                }
            }
            {
                uint8_t first = (state == STATE_SET_HOUR) ? 0 : 2;
                uint8_t vis = (frame & 32) ? 0 : 1;
                show_card(first, vis);
                show_card(first + 1, vis);
            }
        }
    }
}
