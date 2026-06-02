/*
 * adc_filter.h
 * ADC sensor moving average filter - header file
 * Struct definitions, constants, function prototypes
 */

#ifndef ADC_FILTER_H
#define ADC_FILTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Constants */
#define FILTER_SIZE         8
#define MAX_SAMPLES         50
#define ADC_MAX             1023
#define ADC_MIN             0
#define REPORT_FILENAME     "adc_report.txt"
#define CSV_FILENAME        "adc_data.csv"

/*
 * Bit mask definitions (sensor status flags - 8-bit register)
 *  Bit 7: SENSOR_ACTIVE   - sensor active
 *  Bit 6: FILTER_READY    - filter buffer sufficiently filled
 *  Bit 5: OVERRANGE_ERR   - over range error
 *  Bit 4: UNDERRANGE_ERR  - under range error
 *  Bit 3: DATA_VALID      - data valid
 *  Bit 2: BUFFER_FULL     - circular buffer full
 *  Bit 1: (reserved)
 *  Bit 0: INIT_DONE       - initialization complete
 */
#define FLAG_SENSOR_ACTIVE  (1 << 7)
#define FLAG_FILTER_READY   (1 << 6)
#define FLAG_OVERRANGE_ERR  (1 << 5)
#define FLAG_UNDERRANGE_ERR (1 << 4)
#define FLAG_DATA_VALID     (1 << 3)
#define FLAG_BUFFER_FULL    (1 << 2)
#define FLAG_INIT_DONE      (1 << 0)

/* Circular buffer struct (1D array based) */
typedef struct {
    int   buffer[FILTER_SIZE];
    int   head;
    int   count;
    long  sum;
} CircularBuffer;

/* Sensor record struct */
typedef struct {
    int           sensor_id;
    int           raw_value;
    double        filtered_value;
    char          timestamp[32];
    unsigned char status_flags;
} SensorRecord;

/* Statistics struct */
typedef struct {
    int    total_samples;
    int    valid_samples;
    int    error_count;
    double raw_min;
    double raw_max;
    double raw_avg;
    double filtered_min;
    double filtered_max;
    double filtered_avg;
    double noise_reduction;
} Statistics;

/* Initialization */
void init_circular_buffer(CircularBuffer *cb);
void init_sensor_record(SensorRecord *rec, int sensor_id);

/* Bit operations */
void        set_flag(unsigned char *flags, unsigned char mask);
void        clear_flag(unsigned char *flags, unsigned char mask);
int         check_flag(unsigned char flags, unsigned char mask);
const char *decode_status(unsigned char flags);

/* Data input and validation */
int  acquire_sensor_data(int *value);
int  validate_range(int value, unsigned char *flags);

/* Filter algorithm */
void   push_to_buffer(CircularBuffer *cb, int value);
double compute_moving_average(const CircularBuffer *cb);

/* Data processing */
void process_sample(SensorRecord *records, int index,
                    CircularBuffer *cb, int raw_val);

/* Output */
void print_header(void);
void print_record(const SensorRecord *rec, int index);
void print_buffer_state(const CircularBuffer *cb);

/* Statistics */
void compute_statistics(const SensorRecord *records,
                        int count, Statistics *stats);
void print_statistics(const Statistics *stats);

/* File I/O */
int save_report_txt(const SensorRecord *records, int count,
                    const Statistics *stats);
int save_data_csv(const SensorRecord *records, int count);

/* Utility */
void get_timestamp(char *buf, size_t size);
void print_bit_pattern(unsigned char val);

#endif /* ADC_FILTER_H */
