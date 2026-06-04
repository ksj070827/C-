/*
 * adc_filter.c
 * ADC 센서 데이터 보정을 위한 이동 평균 필터 - 핵심 로직 구현
 *
 * 구현 기능:
 *  1. 비트 연산 및 마스킹  - 센서 상태 플래그 레지스터 관리
 *  2. 배열 (1D 순환 버퍼) - 시계열 데이터 저장
 *  3. 구조체 (struct)     - 센서 레코드, 통계, 버퍼 규격화
 *  4. 함수 모듈화         - 입력/필터/출력/저장 분리
 *  5. 포인터              - 함수 간 구조체 참조 전달
 *  6. 파일 입출력         - .txt 보고서 / .csv 데이터 저장
 */

#include "adc_filter.h"

/* =========================================================
 * [기능 1] 비트 연산 및 마스킹 - 상태 플래그 관리
 * ========================================================= */

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
    printf("  상태 레지스터: 0b");
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

/* =========================================================
 * [기능 2] 배열 - 순환 버퍼(Circular Buffer) 구현
 * ========================================================= */

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

/* =========================================================
 * [기능 3] 구조체 - 초기화 함수
 * ========================================================= */

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

/* =========================================================
 * [기능 4] 함수 모듈화 - 데이터 획득, 검증, 처리
 * ========================================================= */

void get_timestamp(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%H:%M:%S", t);
}

int acquire_sensor_data(int *value)
{
    char buf[64];
    printf("\n  ADC 값 입력 (0~1023, 'q'=종료): ");
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
        printf("  [!] 범위 초과 에러: %d > %d (상태: ", value, ADC_MAX);
        print_bit_pattern(*flags);
        printf("  )\n");
        return 0;
    }
    if (value < ADC_MIN) {
        set_flag(flags, FLAG_UNDERRANGE_ERR);
        printf("  [!] 범위 미달 에러: %d < %d\n", value, ADC_MIN);
        return 0;
    }
    set_flag(flags, FLAG_DATA_VALID);
    return 1;
}

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
