/*
 * main.c
 * ADC sensor moving average filter - main entry point
 *
 * Signal model:
 *   - Input range: 482 ~ 543 fixed
 *   - Total samples: 50
 *   - Filter goal: remove noise, restore center value (~512)
 */

#include "adc_filter.h"

#define INPUT_MIN   482
#define INPUT_MAX   543
#define INPUT_RANGE (INPUT_MAX - INPUT_MIN)

static int generate_range_value(void)
{
    return INPUT_MIN + (rand() % (INPUT_RANGE + 1));
}

int main(void)
{
    SensorRecord   records[MAX_SAMPLES];
    CircularBuffer cb;
    Statistics     stats;

    int i;
    int sample_count = 0;
    int raw_value    = 0;

    srand((unsigned int)time(NULL));

    init_circular_buffer(&cb);
    for (i = 0; i < MAX_SAMPLES; i++)
        init_sensor_record(&records[i], 1);

    printf("\n");
    printf("  ==========================================\n");
    printf("  ADC Moving Average Filter Simulator\n");
    printf("  [Signal model] Fixed range input\n");
    printf("  ==========================================\n");
    printf("  - Input range : %d ~ %d (width: %d)\n", INPUT_MIN, INPUT_MAX, INPUT_RANGE);
    printf("  - Filter size : %d-sample moving average\n", FILTER_SIZE);
    printf("  - Total samples: %d\n", MAX_SAMPLES);
    printf("  ==========================================\n");

    printf("\n  [Status register bit structure]\n");
    printf("  Bit7=ACTIVE | Bit6=FILTER_RDY | Bit5=OVER_ERR | Bit4=UNDER_ERR\n");
    printf("  Bit3=VALID  | Bit2=BUF_FULL   | Bit1=(reserved)| Bit0=INIT_DONE\n");

    print_header();

 while (sample_count < MAX_SAMPLES)
 {
     raw_value = generate_range_value();

     printf("  [#%3d] Input: %4d\n", sample_count + 1, raw_value);

     process_sample(records, sample_count, &cb, raw_value);

     print_record(&records[sample_count], sample_count);
     print_bit_pattern(records[sample_count].status_flags);
     print_buffer_state(&cb);

     sample_count++;

     if (sample_count == FILTER_SIZE)
         printf("\n  ** Filter buffer full! Full moving average now active **\n\n");
 }
printf("  ==========================================\n");
printf("\n  Total %d samples processed.\n", sample_count);

compute_statistics(records, sample_count, &stats);
print_statistics(&stats);

printf("\n  [Saving files...]\n");
printf("  Save path: ./ (current directory)\n\n");

if (save_report_txt(records, sample_count, &stats))
    printf("  [OK] Report saved -> ./%s\n", REPORT_FILENAME);
else
    printf("  [FAIL] Report save failed\n");

if (save_data_csv(records, sample_count))
    printf("  [OK] CSV saved -> ./%s\n", CSV_FILENAME);
else
    printf("  [FAIL] CSV save failed\n");

printf("\n  Program complete.\n\n");
return 0;
}
