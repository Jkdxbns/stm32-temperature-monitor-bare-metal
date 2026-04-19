// Include necessary library
#include "stm32f4xx.h"

/*--------------------------------------  DECLARING VARIABLES  -------------------------------------------------*/
/* --------------------------  ADC VARS  --------------------------  */
volatile uint16_t adc_raw[2] = {0, 0};
volatile uint8_t adc_pair_ready = 0;

/* --------------------------  OLED VARS  --------------------------  */
uint8_t oled_addr = 0x3C;
static const uint8_t font5x7_digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 30x3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}  // 9
};
static const uint8_t font_L[5] = {0x7F,0x40,0x40,0x40,0x40};
static const uint8_t font_M[5] = {0x7F,0x02,0x04,0x02,0x7F};
static const uint8_t font_F[5] = {0x7F,0x09,0x09,0x09,0x01};
static const uint8_t font_dash[5]  = {0x08,0x08,0x08,0x08,0x08};
static const uint8_t font_colon[5] = {0x00,0x36,0x36,0x00,0x00};
static const uint8_t font_space[5] = {0x00,0x00,0x00,0x00,0x00};

/* --------------------------------------  DECLARING FUNCTIONS  -------------------------------------------------*/
/* --------------------------  I2C HANDLERS  --------------------------  */
static void i2c1_set_start(void){
    while (I2C1->SR2 & (1U << 1)) {;}   	// wait while BUSY=1
    I2C1->CR1 |= (1U << 8);             	// START=1
    while (!(I2C1->SR1 & (1U << 0))) {;} 	// wait SB=1
}
static void i2c1_set_stop(void){
	I2C1->CR1 |= (1U << 9);      // STOP
}
static uint8_t i2c1_af_handler(void){
    if (I2C1->SR1 & (1U << 10)) {    // AF
    	i2c1_set_stop();
        I2C1->SR1 &= ~(1U << 10);    // clear AF
        return 1;
    }
    return 0;
}
static uint8_t i2c1_send_addr(uint8_t addr){
    I2C1->DR = (addr << 1);   // 7-bit addr + write bit
    while (!(I2C1->SR1 & (1U << 1))) {if(i2c1_af_handler()) return 0;}

    volatile uint32_t temp;
    temp = I2C1->SR1;   // clear ADDR
    temp = I2C1->SR2;
    (void)temp;

    return 1;
}
static uint8_t i2c1_write(uint8_t data_byte){
    while (!(I2C1->SR1 & (1U << 7))) {if(i2c1_af_handler()) return 0;}
    I2C1->DR = data_byte;
    while (!(I2C1->SR1 & (1U << 2))) {if(i2c1_af_handler()) return 0;}
    return 1;
}
static void i2c1_end(void){
	while (!(I2C1->SR1 & (1U << 2))) {;}   // wait BTF
	i2c1_set_stop();
}

/*-----------------  HELPER FUNCTIONS  -----------------*/
static uint32_t adc_temp_f(uint16_t adc_val){	// Convert ADC raw data to Fahrenheit
	// LM35: 10mV / °C
	// Vref = 3.3V (3300mV), ADC max = 4095 (4096-1)
	// Temp = (adc_val * Vref_mV) / ADC_res /10 (as 10mV per C)
	// Since we don't use float Celsius is bad thus Fahrenheit
	uint32_t mv = ((uint32_t)adc_val * 3300U) / 4095U;
	uint32_t c  = mv / 10U;
	return (c * 9U / 5U) + 32U;
}
static void temp_to_2digits(uint8_t t, char *out){
    out[0] = '0' + ((t / 10U) % 10U);
    out[1] = '0' + (t % 10U);
}

/* --------------------------  OLED HANDLERS  --------------------------  */
static uint8_t oled_write_command(uint8_t cmd){
    i2c1_set_start();

    if (!i2c1_send_addr(oled_addr)) return 0;   // OLED 7-bit address
    /* control byte: command
     * byte required = 0b00(Co)(C/D)00'0000
     * since we send data byte with data we set C0=0 and C/D=Data=0
     */
    if (!i2c1_write(0x00))     return 0;
    if (!i2c1_write(cmd))      return 0;   // actual command
    i2c1_end();
    return 1;
}
static void oled_init(void){
    oled_write_command(0xAE); // display OFF
    oled_write_command(0xD5); oled_write_command(0x80); // clock divide / osc
    oled_write_command(0xA8); oled_write_command(0x3F); // multiplex = 64
    oled_write_command(0xD3); oled_write_command(0x00); // display offset = 0
    oled_write_command(0x40);                           // start line = 0
    oled_write_command(0x8D); oled_write_command(0x14); // charge pump ON
    oled_write_command(0x20); oled_write_command(0x00); // horizontal addressing
    oled_write_command(0xA1);                           // segment remap
    oled_write_command(0xC8);                           // COM scan dec
    oled_write_command(0xDA); oled_write_command(0x12); // COM pins config for 128x64
    oled_write_command(0x81); oled_write_command(0x7F); // contrast
    oled_write_command(0xD9); oled_write_command(0xF1); // pre-charge
    oled_write_command(0xDB); oled_write_command(0x40); // VCOMH
    oled_write_command(0xA4);                           // resume RAM display
    oled_write_command(0xA6);                           // normal display
    oled_write_command(0xAF);                           // display ON
}
static void oled_set_cursor(uint8_t page, uint8_t col){
    oled_write_command(0x21);              // Set Column Address
    oled_write_command(col);               // start column
    oled_write_command(127);               // end column

    oled_write_command(0x22);              // Set Page Address
    oled_write_command(page);              // start page
    oled_write_command(page);              // end page
}
static uint8_t oled_write_data_stream(const uint8_t *buf, uint16_t len){
    i2c1_set_start();

    if (!i2c1_send_addr(oled_addr)) return 0;
    if (!i2c1_write(0x40)) return 0;   // control byte: data stream

    for (uint16_t i = 0; i < len; i++) {
        if (!i2c1_write(buf[i])) return 0;
    }

    i2c1_end();
    return 1;
}
static void oled_draw_glyph5(const uint8_t glyph[5]){
    uint8_t out[6];

    out[0] = glyph[0];
    out[1] = glyph[1];
    out[2] = glyph[2];
    out[3] = glyph[3];
    out[4] = glyph[4];
    out[5] = 0x00;   // spacing

    oled_write_data_stream(out, 6);
}
static void oled_clear_page(uint8_t page){
    uint8_t blank[128] = {0};

    oled_set_cursor(page, 0);
    oled_write_data_stream(blank, 128);
}
static void oled_draw_char(char c){
    if (c >= '0' && c <= '9') {
        oled_draw_glyph5(font5x7_digits[c - '0']);
    } else if (c == 'L') {
        oled_draw_glyph5(font_L);
    } else if (c == 'M') {
        oled_draw_glyph5(font_M);
    } else if (c == 'F') {
        oled_draw_glyph5(font_F);
    } else if (c == '-') {
        oled_draw_glyph5(font_dash);
    } else if (c == ':') {
        oled_draw_glyph5(font_colon);
    } else {
        oled_draw_glyph5(font_space);
    }
}
static void oled_draw_text(uint8_t page, uint8_t col, const char *s){
    oled_set_cursor(page, col);
    while (*s) {
        oled_draw_char(*s++);
    }
}
static void oled_show_temps(uint8_t t1, uint8_t t2){
    char line1[] = "LM35-1: 00F";
    char line2[] = "LM35-2: 00F";

    temp_to_2digits(t1, &line1[8]);
    temp_to_2digits(t2, &line2[8]);

    oled_clear_page(0);
    oled_clear_page(1);

    oled_draw_text(0, 0, line1);
    oled_draw_text(1, 0, line2);
}


/*-----------------  ISR HANDLERS  -----------------*/
void ADC_IRQHandler(void){	// ADC Handler
	static uint8_t seq_idx = 0;			// Tracks first or second conversion in the sequence

    if (ADC1->SR & (1U << 1)) {      	// To confirm if EOC was only the one to call it
    	adc_raw[seq_idx] = (uint16_t)ADC1->DR;
    	seq_idx ++;

        if (seq_idx >= 2U) {
            seq_idx = 0;
            adc_pair_ready = 1;
        }
    }
}


int main(void) {
	/*--------------------------------------  DECLARING VARIABLES  -------------------------------------------------*/
	uint8_t temp6_f = 0xFF, temp7_f = 0xFF;
	uint8_t prev_temp6_f = 0xFF, prev_temp7_f = 0xFF;

	/*--------------------------------------  TIMER-2 CONFIGURATION  -------------------------------------------------*/
    // Enable clock for GPIOA
    RCC->AHB1ENR |= (1U << 0);  // GPIOA EN
    // Enable timer 2
    RCC->APB1ENR |= (1U << 0);  // TIM2 EN
    // Configure TIM2
    TIM2->PSC = 15999U;     	// Pre-scaler to set clock at 1KHz(reduces clock speed)
    TIM2->ARR = 999U;      		// Auto-reload at 1s(determines frequency)
    TIM2->CCR1 = 500U;     		// Compare match value at 500ms(determines toggle point)

    /*-----------------  set timer for LED -----------------*/
    // Set OC1M bits to 011 (Toggle mode) in CCMR1
    // Bits 6:4 = 011
    TIM2->CCMR1 &= ~(7U << 4);
    TIM2->CCMR1 |=  (3U << 4);
    // Enable Channel 1 output (CC1E)
    TIM2->CCER |= 	(1U << 0);

    /*-------------  set timer trigger for ADC --------------*/
    TIM2->CR2 &= ~(7U << 4);		// Clear MMS garbage
    TIM2->CR2 |= (2U  << 4);		// Set to update event

	/*---------------------------------------  GPIO CONFIGURATION  -------------------------------------------------*/
    /*----------------- LED -----------------*/
    // Enable GPIOA by clearing and setting
    GPIOA->MODER &= ~(3U << (5 * 2));
    GPIOA->MODER |=  (2U << (5 * 2));
    // To use TIM2->CH1 we must set PA5 to AF1
    GPIOA->AFR[0] &= ~(0xFU << (5 * 4));
    GPIOA->AFR[0] |=  (0x1U << (5 * 4));
    // Set Pin switching state to high speed for clean simulation edges
    GPIOA->OSPEEDR |= (2U << (5 * 2));

    /*----------------- LM35 ----------------*/
    GPIOA->MODER &= ~(3U << (6 * 2));	// CH6
    GPIOA->MODER |=  (3U << (6 * 2));	// Analog mode
    GPIOA->PUPDR &= ~(3U << (6 * 2));	// Clear pull-up pull-down states
    GPIOA->MODER &= ~(3U << (7 * 2));	// CH7
    GPIOA->MODER |=  (3U << (7 * 2));	// Analog mode
    GPIOA->PUPDR &= ~(3U << (7 * 2)); 	// Clear pull-up pull-down states

    /*----------------- I2C ----------------*/
    // Enable GPIOB
    RCC->AHB1ENR |= (1u << 1);
    // Set pin 8 and 9 mode for alternate function
    GPIOB->MODER &= ~(3U << (8 * 2));	// Pin 8
    GPIOB->MODER |=  (2U << (8 * 2));
    GPIOB->MODER &= ~(3U << (9 * 2));	// Pin 9
    GPIOB->MODER |=  (2U << (9 * 2));
    // Set what kind of alternate function
    GPIOB->AFR[1] &= ~(0XFU << 0);		// Pin 8
    GPIOB->AFR[1] |= 	(4U << 0);
    GPIOB->AFR[1] &= ~(0XFU << 4);		// Pin 9
    GPIOB->AFR[1] |= 	(4U << 4);
    // For I2C we open drain the pins
    GPIOB->OTYPER |= (1U << 8);			// Pin 8
	GPIOB->OTYPER |= (1U << 9);			// Pin 9
	// No pull-up pull-down
	GPIOB->PUPDR &= ~(3U << (8 * 2));	// Pin 8
	GPIOB->PUPDR &= ~(3U << (9 * 2));	// Pin 9
	// Set pin toggle speed to medium
	GPIOB->OSPEEDR &= ~(3U << (8 * 2));	// Pin 8
	GPIOB->OSPEEDR |=  (1U << (8 * 2));
	GPIOB->OSPEEDR &= ~(3U << (9 * 2));	// Pin 9
	GPIOB->OSPEEDR |=  (1U << (9 * 2));

	/*----------------------------------------  ADC CONFIGURATION  -------------------------------------------------*/
    // Enable ADC clock
    RCC->APB2ENR |= (1U << 8);
    // Set common ADC clock
    ADC->CCR &= ~(3U << 16);
    ADC->CCR |=  (1U << 16);
    // Set ADC resolution
    ADC1->CR1 &= ~(3U << 24);
    // Set ADC to single mode
    ADC1->CR2 &= ~(1U << 1);
    // Set data storage assignment
    ADC1->CR2 &= ~(1U << 11);
    // We enable external trigger
    ADC1->CR2 &= ~(3U << 28);
    ADC1->CR2 |=  (1U << 28);
    // We set timer 2 TRG) as external trigger
    ADC1->CR2 &= ~(0xFU << 24);
	ADC1->CR2 |=  (6U   << 24);
	// We want EOC set after each conversion so we can have both values
	// If not then only the next conversion overwrites previous data
	ADC1->CR2 |= (1U << 10);
	// We need scan mode to sample entire sequence else it will stop after 1st conversion
	ADC1->CR1 |= (1U << 8);
	// Enable EOC interrupt so we run ISR for sample to temperature conversion
	ADC1->CR1 |= (1U << 5);
    // set sample rate
    ADC1->SMPR2 &= ~(7U << (6 * 3));	// ch6
    ADC1->SMPR2 |=  (7U << (6 * 3));
    ADC1->SMPR2 &= ~(7U << (7 * 3));	// CH7
    ADC1->SMPR2 |=  (7U << (7 * 3));
    // Set conversion sequence, how many to convert, list
    ADC1->SQR1 &= ~(0xFU << 20);	// clear garbage
    ADC1->SQR1 |=  (1U   << 20);
    // Define the order of conversion
    ADC1->SQR3 &= ~(0x1F << 0);		// clear 1st conversion bits
    ADC1->SQR3 |=  (0x6U << 0);		// set ch6 as first conversion
    ADC1->SQR3 &= ~(0x1F << 5);
    ADC1->SQR3 |=  (0x7U << 5);		// set ch7 as second conversion
    // Use ADC
    ADC1->CR2 |= (1U << 0);
	/* We need to enable ADC_IRQ using NVIC which is done after ADC is fully configured
	 * We also create an ISR, for ADC it is ADC_IRQHandler
	 * Same as `NVIC->ISER[0] = (1U << 18);` as ADC is priority 25 but positioned at 18 in the vector
	 * Keep this in mind that this vector is exclusive for STM32F01RE that ADC priority is 25
	*/
	NVIC->ISER[0] |= (1U << ADC_IRQn);
	// We need to wait for tSTAB before ADC stabilizes thus we add a simple loop

	/*----------------------------------------  I2C CONFIGURATION  -------------------------------------------------*/
	// Enable I2C1
	RCC->APB1ENR |= (1U << 21);
	// We set the PSCLK1 value to 16MHz
	I2C1->CR2 &= ~(0x3FU << 0);
	I2C1->CR2 |=  (16U   << 0);
	// We set I2C to standard mode
	I2C1->CCR &= ~(1U << 15); 			// F/S = 0
	/* Set clock control to match Standard mode speed
	 * CCR = T_psclk1 / (2 x F_scl)
	 * F_scl = 100KHz, T_psclk1 = 16MHz
	 */
	I2C1->CCR &= ~(0XFFFU << 0);		// Clear CCR
	I2C1->CCR |=  (0X50   << 0);		// CCR = 80 decimal that is 0X50
	/* We set the TRISE value
	 * TRISE = tr_max / T_psclk1
	 * tr_max = 1000ns for standard, T_psclk1 = 16MHz
	 */
	I2C1->TRISE &= ~(0X3F << 0);
	I2C1->TRISE |=  (0X11 << 0);		// TRISE = 17 decimal that is 0X11
	// Chip necessity for I2C
	I2C1->OAR1 |= (1U << 14);			// As per datasheet must always be kept 1 by software
	// Enable I2C peripheral
	I2C1->CR1 |= (1U << 0);
	// Delay for I2c to stabilize
	for (volatile uint32_t i = 0; i < 100U; i++) {;}

	/*----------------------------------  START TIMER TO GET EVERYTHONG WORKING  -----------------------------------*/
    // Start timer 2
    TIM2->EGR |= (1U << 0);   // Force update to load PSC and ARR immediately
    TIM2->CR1 |= (1U << 0);   // Enable Counter (CEN)

	/*----------------------------------------  OLED CONFIGURATION  -------------------------------------------------*/
	oled_init();
//	oled_set_cursor(0, 0);
//	oled_draw_char('5');
//	while(1){;}


    while (1) {
        if (adc_pair_ready) {
            adc_pair_ready = 0;

            temp6_f = (uint8_t)adc_temp_f(adc_raw[0]);
            temp7_f = (uint8_t)adc_temp_f(adc_raw[1]);

            if ((temp6_f != prev_temp6_f) || (temp7_f != prev_temp7_f)) {
                prev_temp6_f = temp6_f;
                prev_temp7_f = temp7_f;

                oled_show_temps(temp6_f, temp7_f);

                (void)temp6_f, (void)temp7_f;
            }
        }
    }
}
