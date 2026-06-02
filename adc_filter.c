/*
 * adc_filter.c
 * ADC sensor moving average filter - core logic implementation
 *
 * Features:
 *  1. Bit operations and masking
 *  2. Array (1D circular buffer)
 *  3. Struct
 *  4. Function modularization
 *  5. Pointer
 *  6. File I/O
 */

#include "adc_filter.h"

/* [Feature 1] Bit operations */

void set_flag(unsigned char *flags, unsigned char mask)
{
    *flags |= mask;
}

void clear_flag(unsigned char *flags, unsigned char mask)
{
    *flags &= ~mask;
}

int check_flag(unsigned char flags, unsigned char mask)
{
    return (flags & mask) ? 1 : 0;
}

void print_bit_pattern(unsigned char val)
{
    int i;
    printf("  Status register: 0b");
    for (i = 7; i >= 0; i--) {
        printf("%d", (val >> i) & 1);
        if (i == 4) printf("_");
    }
    printf(" (0x%02X)\n", val);
}

const char *decode_status(unsigned char flags)
{
    if (check_flag(flags, FLAG_OVERRANGE_ERR))  return "ERROR:OVER_RANGE";
    if (check_flag(flags, FLAG_UNDERRANGE_ERR)) return "ERROR:UNDER_RANGE";
    if (!check_flag(flags, FLAG_DATA_VALID))    return "INVALID";
    if (check_flag(flags, FLAG_FILTER_READY))   return "FILTERED_OK";
    return "BUFFERING";
}

/* [Feature 2] Circular buffer */

void init_circular_buffer(CircularBuffer *cb)
{
    int i;
    cb->head  = 0;
    cb->count = 0;
    cb->sum   = 0;
    for (i = 0; i < FILTER_SIZE; i++)
        cb->buffer[i] = 0;
}

void push_to_buffer(CircularBuffer *cb, int value)
{
    if (cb->count == FILTER_SIZE) {
        cb->sum -= cb->buffer[cb->head];
    } else {
        cb->count++;
    }
    cb->buffer[cb->head] = value;
    cb->sum += value;
    cb->head = (cb->head + 1) % FILTER_SIZE;
}

double compute_moving_average(const CircularBuffer *cb)
{
    if (cb->count == 0) return 0.0;
    return (double)cb->sum / (double)cb->count;
}

/* [Feature 3] Struct initialization */

void init_sensor_record(SensorRecord *rec, int sensor_id)
{
    rec->sensor_id      = sensor_id;
    rec->raw_value      = 0;
    rec->filtered_value = 0.0;
    rec->status_flags   = 0x00;
    memset(rec->timestamp, 0, sizeof(rec->timestamp));
    set_flag(&rec->status_flags, FLAG_INIT_DONE);
    set_flag(&rec->status_flags, FLAG_SENSOR_ACTIVE);
}

/* [Feature 4] Function modularization */

void get_timestamp(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%H:%M:%S", t);
}

int acquire_sensor_data(int *value)
{
    char buf[64];
    printf("\n  Enter ADC value (0~1023, q=quit): ");
    if (fgets(buf, sizeof(buf), stdin) == NULL) return 0;
    if (buf[0] == 'q' || buf[0] == 'Q') return 0;
    *value = atoi(buf);
    return 1;
}

int validate_range(int value, unsigned char *flags)
{
    clear_flag(flags, FLAG_OVERRANGE_ERR);
    clear_flag(flags, FLAG_UNDERRANGE_ERR);
    clear_flag(flags, FLAG_DATA_VALID);

    if (value > ADC_MAX) {
        set_flag(flags, FLAG_OVERRANGE_ERR);
        printf("  [!] Over range error: %d > %d\n", value, ADC_MAX);
        return 0;
    }
    if (value < ADC_MIN) {
        set_flag(flags, FLAG_UNDERRANGE_ERR);
        printf("  [!] Under range error: %d < %d\n", value, ADC_MIN);
        return 0;
    }
    set_flag(flags, FLAG_DATA_VALID);
    return 1;
}

/* [Feature 5] Pointer usage */

void process_sample(SensorRecord *records, int index,
                    CircularBuffer *cb, int raw_val)
{
    SensorRecord *rec = &records[index];
    unsigned char tmp_flags = rec->status_flags;

    if (!validate_range(raw_val, &tmp_flags)) {
        rec->status_flags = tmp_flags;
        rec->raw_value    = raw_val;
        get_timestamp(rec->timestamp, sizeof(rec->timestamp));
        return;
    }

    push_to_buffer(cb, raw_val);

    if (cb->count == FILTER_SIZE) {
        set_flag(&tmp_flags, FLAG_FILTER_READY);
        set_flag(&tmp_flags, FLAG_BUFFER_FULL);
    }

    rec->raw_value      = raw_val;
    rec->filtered_value = compute_moving_average(cb);
    rec->status_flags   = tmp_flags;
    get_timestamp(rec->timestamp, sizeof(rec->timestamp));
}

/* Console output */

void print_header(void)
{
    printf("\n");
    printf("  ==========================================\n");
    printf("  ADC Moving Average Filter Simulator (window: %2d)\n", FILTER_SIZE);
    printf("  ==========================================\n");
    printf("  # | Raw  | Filtered | Diff   | Status          | Time\n");
    printf("  ==========================================\n");
}

void print_record(const SensorRecord *rec, int index)
{
    double diff = rec->raw_value - rec->filtered_value;
    printf("  %3d | %4d | %6.1f | %+6.1f | %-15s | %s\n",
           index + 1,
           rec->raw_value,
           rec->filtered_value,
           diff,
           decode_status(rec->status_flags),
           rec->timestamp);
}

void print_buffer_state(const CircularBuffer *cb)
{
    int i, pos;
    printf("  [Buffer] Count: %d/%d | Sum: %ld | Avg: %.1f\n",
           cb->count, FILTER_SIZE, cb->sum,
           compute_moving_average(cb));
    printf("  [Buffer contents] ");
    for (i = 0; i < FILTER_SIZE; i++) {
        pos = ((cb->head - 1 - i) + FILTER_SIZE * 2) % FILTER_SIZE;
        if (i < cb->count)
            printf("[%d]", cb->buffer[pos]);
        else
            printf("[ - ]");
        if (i < FILTER_SIZE - 1) printf(" -> ");
    }
    printf(" (newest->oldest)\n");
}

/* Statistics */

void compute_statistics(const SensorRecord *records,
                        int count, Statistics *stats)
{
    int i;
    double raw_sum  = 0.0;
    double filt_sum = 0.0;
    double raw_var  = 0.0;
    double filt_var = 0.0;
    double d_raw    = 0.0;
    double d_filt   = 0.0;
    double raw_std  = 0.0;
    double filt_std = 0.0;

    memset(stats, 0, sizeof(Statistics));
    if (count == 0) return;

    stats->total_samples = count;
    stats->raw_min       = records[0].raw_value;
    stats->raw_max       = records[0].raw_value;
    stats->filtered_min  = records[0].filtered_value;
    stats->filtered_max  = records[0].filtered_value;

    for (i = 0; i < count; i++) {
        const SensorRecord *r = &records[i];
        if (check_flag(r->status_flags, FLAG_OVERRANGE_ERR) ||
            check_flag(r->status_flags, FLAG_UNDERRANGE_ERR)) {
            stats->error_count++;
            continue;
        }
        stats->valid_samples++;
        raw_sum  += r->raw_value;
        filt_sum += r->filtered_value;
        if (r->raw_value      < stats->raw_min)      stats->raw_min      = r->raw_value;
        if (r->raw_value      > stats->raw_max)      stats->raw_max      = r->raw_value;
        if (r->filtered_value < stats->filtered_min) stats->filtered_min = r->filtered_value;
        if (r->filtered_value > stats->filtered_max) stats->filtered_max = r->filtered_value;
    }

    if (stats->valid_samples == 0) return;

    stats->raw_avg      = raw_sum  / stats->valid_samples;
    stats->filtered_avg = filt_sum / stats->valid_samples;

    for (i = 0; i < count; i++) {
        const SensorRecord *r = &records[i];
        if (check_flag(r->status_flags, FLAG_OVERRANGE_ERR) ||
            check_flag(r->status_flags, FLAG_UNDERRANGE_ERR)) continue;
        d_raw  = r->raw_value      - stats->raw_avg;
        d_filt = r->filtered_value - stats->filtered_avg;
        raw_var  += d_raw  * d_raw;
        filt_var += d_filt * d_filt;
    }
    raw_var  /= stats->valid_samples;
    filt_var /= stats->valid_samples;
    raw_std  = sqrt(raw_var);
    filt_std = sqrt(filt_var);

    if (raw_std > 0.0)
        stats->noise_reduction = (1.0 - filt_std / raw_std) * 100.0;
    else
        stats->noise_reduction = 0.0;
}

void print_statistics(const Statistics *stats)
{
    printf("\n  ==========================================\n");
    printf("  Statistics Summary\n");
    printf("  ==========================================\n");
    printf("  Total: %-5d  | Valid: %-5d\n",
           stats->total_samples, stats->valid_samples);
    printf("  Errors: %-5d\n", stats->error_count);
    printf("  ==========================================\n");
    printf("  [Raw]      min: %6.1f | max: %6.1f | avg: %6.1f\n",
           stats->raw_min, stats->raw_max, stats->raw_avg);
    printf("  [Filtered] min: %6.1f | max: %6.1f | avg: %6.1f\n",
           stats->filtered_min, stats->filtered_max, stats->filtered_avg);
    printf("  ==========================================\n");
    printf("  Noise reduction: %+.2f %%\n", stats->noise_reduction);
    printf("  ==========================================\n");
}

/* [Feature 6] File I/O */

int save_report_txt(const SensorRecord *records, int count,
                    const Statistics *stats)
{
    int i;
    FILE *fp = fopen(REPORT_FILENAME, "w");
    if (!fp) {
        perror("  [Error] Failed to open report file");
        return 0;
    }

    fprintf(fp, "====================================================\n");
    fprintf(fp, "  ADC Sensor Moving Average Filter Simulation Report\n");
    fprintf(fp, "  Filter window size: %d\n", FILTER_SIZE);
    fprintf(fp, "====================================================\n\n");

    fprintf(fp, "%-4s %-8s %-10s %-8s %-18s %-8s\n",
            "No.", "Raw", "Filtered", "Diff", "Status", "Time");
    fprintf(fp, "----------------------------------------------------\n");

    for (i = 0; i < count; i++) {
        const SensorRecord *r = &records[i];
        double diff = r->raw_value - r->filtered_value;
        int b;
        unsigned char v;
        fprintf(fp, "%-4d %-8d %-10.2f %-+8.2f %-18s %-8s\n",
                i + 1, r->raw_value, r->filtered_value,
                diff, decode_status(r->status_flags), r->timestamp);
        fprintf(fp, "     Status(HEX): 0x%02X  BIN: 0b", r->status_flags);
        v = r->status_flags;
        for (b = 7; b >= 0; b--) fprintf(fp, "%d", (v >> b) & 1);
        fprintf(fp, "\n");
    }

    fprintf(fp, "\n====================================================\n");
    fprintf(fp, "  Statistics Summary\n");
    fprintf(fp, "====================================================\n");
    fprintf(fp, "  Total samples : %d\n", stats->total_samples);
    fprintf(fp, "  Valid samples : %d\n", stats->valid_samples);
    fprintf(fp, "  Error count   : %d\n", stats->error_count);
    fprintf(fp, "  Raw    (min/max/avg) : %.1f / %.1f / %.1f\n",
            stats->raw_min, stats->raw_max, stats->raw_avg);
    fprintf(fp, "  Filter (min/max/avg) : %.1f / %.1f / %.1f\n",
            stats->filtered_min, stats->filtered_max, stats->filtered_avg);
    fprintf(fp, "  Noise reduction      : %.2f %%\n", stats->noise_reduction);
    fprintf(fp, "====================================================\n");

    fclose(fp);
    return 1;
}

int save_data_csv(const SensorRecord *records, int count)
{
    int i;
    FILE *fp = fopen(CSV_FILENAME, "w");
    if (!fp) {
        perror("  [Error] Failed to open CSV file");
        return 0;
    }

    fprintf(fp, "No,SensorID,RawValue,FilteredValue,Difference,"
                "StatusHex,StatusStr,Timestamp\n");

    for (i = 0; i < count; i++) {
        const SensorRecord *r = &records[i];
        double diff = r->raw_value - r->filtered_value;
        fprintf(fp, "%d,%d,%d,%.2f,%.2f,0x%02X,%s,%s\n",
                i + 1, r->sensor_id, r->raw_value,
                r->filtered_value, diff, r->status_flags,
                decode_status(r->status_flags), r->timestamp);
    }

    fclose(fp);
    return 1;
}
