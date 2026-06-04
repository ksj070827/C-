/*
 * adc_filter.h
 * ADC 센서 데이터 보정을 위한 이동 평균 필터 설계 및 시뮬레이션
 * 헤더 파일: 구조체 정의, 상수, 함수 프로토타입
 */

#ifndef ADC_FILTER_H
#define ADC_FILTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* =========================================================
 * 상수 정의
 * ========================================================= */
#define FILTER_SIZE         8       /* 이동 평균 윈도우 크기 (순환 버퍼 크기) */
#define MAX_SAMPLES         50     /* 최대 샘플 수 */
#define ADC_MAX             1023    /* ADC 최대값 (10비트) */
#define ADC_MIN             0       /* ADC 최소값 */
#define REPORT_FILENAME     "adc_report.txt"
#define CSV_FILENAME        "adc_data.csv"

/* =========================================================
 * 비트 마스크 정의 (센서 상태 플래그 - 8비트 레지스터)
 * =========================================================
 *  Bit 7: SENSOR_ACTIVE   - 센서 활성화 여부
 *  Bit 6: FILTER_READY    - 필터 버퍼 충분히 채워짐
 *  Bit 5: OVERRANGE_ERR   - 범위 초과 에러
 *  Bit 4: UNDERRANGE_ERR  - 범위 미달 에러
 *  Bit 3: DATA_VALID      - 데이터 유효
 *  Bit 2: BUFFER_FULL     - 순환 버퍼 포화 상태
 *  Bit 1: (예약)
 *  Bit 0: INIT_DONE       - 초기화 완료
 */
#define FLAG_SENSOR_ACTIVE  (1 << 7)  /* 0b10000000 = 0x80 */
#define FLAG_FILTER_READY   (1 << 6)  /* 0b01000000 = 0x40 */
#define FLAG_OVERRANGE_ERR  (1 << 5)  /* 0b00100000 = 0x20 */
#define FLAG_UNDERRANGE_ERR (1 << 4)  /* 0b00010000 = 0x10 */
#define FLAG_DATA_VALID     (1 << 3)  /* 0b00001000 = 0x08 */
#define FLAG_BUFFER_FULL    (1 << 2)  /* 0b00000100 = 0x04 */
#define FLAG_INIT_DONE      (1 << 0)  /* 0b00000001 = 0x01 */

/* =========================================================
 * 구조체 정의
 * ========================================================= */

/* 순환 버퍼 구조체 (1차원 배열 기반) */
typedef struct {
    int   buffer[FILTER_SIZE];  /* 시계열 데이터 저장 버퍼 (1D 배열) */
    int   head;                 /* 다음 쓰기 위치 (인덱스) */
    int   count;                /* 현재 저장된 데이터 수 */
    long  sum;                  /* 합계 (빠른 평균 계산용) */
} CircularBuffer;

/* 센서 레코드 구조체 */
typedef struct {
    int    sensor_id;                   /* 센서 식별자 */
    int    raw_value;                   /* ADC 원본 값 (0~1023) */
    double filtered_value;              /* 이동 평균 필터 적용 후 값 */
    char   timestamp[32];               /* 타임스탬프 문자열 */
    unsigned char status_flags;         /* 8비트 상태 플래그 레지스터 */
} SensorRecord;

/* 분석 통계 구조체 */
typedef struct {
    int    total_samples;       /* 총 샘플 수 */
    int    valid_samples;       /* 유효 샘플 수 */
    int    error_count;         /* 에러 발생 횟수 */
    double raw_min;             /* 원본 최솟값 */
    double raw_max;             /* 원본 최댓값 */
    double raw_avg;             /* 원본 평균 */
    double filtered_min;        /* 필터링 최솟값 */
    double filtered_max;        /* 필터링 최댓값 */
    double filtered_avg;        /* 필터링 평균 */
    double noise_reduction;     /* 노이즈 감소율 (%) */
} Statistics;

/* =========================================================
 * 함수 프로토타입 (모듈화)
 * ========================================================= */

/* 초기화 */
void init_circular_buffer(CircularBuffer *cb);
void init_sensor_record(SensorRecord *rec, int sensor_id);

/* 비트 연산: 상태 플래그 관리 */
void        set_flag(unsigned char *flags, unsigned char mask);
void        clear_flag(unsigned char *flags, unsigned char mask);
int         check_flag(unsigned char flags, unsigned char mask);
const char *decode_status(unsigned char flags);

/* 데이터 입력 및 검증 */
int  acquire_sensor_data(int *value);
int  validate_range(int value, unsigned char *flags);

/* 필터 알고리즘 */
void   push_to_buffer(CircularBuffer *cb, int value);
double compute_moving_average(const CircularBuffer *cb);

/* 데이터 처리 (포인터 활용) */
void process_sample(SensorRecord *records, int index,
                    CircularBuffer *cb, int raw_val);

/* 출력 */
void print_header(void);
void print_record(const SensorRecord *rec, int index);
void print_buffer_state(const CircularBuffer *cb);

/* 통계 분석 */
void compute_statistics(const SensorRecord *records,
                        int count, Statistics *stats);
void print_statistics(const Statistics *stats);

/* 파일 입출력 */
int save_report_txt(const SensorRecord *records, int count,
                    const Statistics *stats);
int save_data_csv(const SensorRecord *records, int count);

/* 유틸리티 */
void get_timestamp(char *buf, size_t size);
void print_bit_pattern(unsigned char val);

#endif /* ADC_FILTER_H */
