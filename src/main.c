/*
 * main.c
 * ADC 센서 데이터 보정을 위한 이동 평균 필터 - 메인 진입점
 *
 * 신호 모델:
 *   - 입력 범위: 482 ~ 543 고정
 *   - 총 샘플 : 50개
 *   - 필터 목표: 노이즈를 제거하여 중심값(약 512)에 가깝게 복원
 */

#include "../include/adc_filter.h"

#define INPUT_MIN   482                       /* 입력값 최솟값          */
#define INPUT_MAX   543                       /* 입력값 최댓값          */
#define INPUT_RANGE (INPUT_MAX - INPUT_MIN)   /* 범위 폭: 61            */

 /* -- 고정 범위 난수 생성 함수 -------------------------------------------
  * 반환값: 482 이상 543 이하의 정수 난수
  *         482 + rand() % 62  ->  482 ~ 543
  * ----------------------------------------------------------------------- */
static int generate_range_value(void)
{
    return INPUT_MIN + (rand() % (INPUT_RANGE + 1));  /* 482 ~ 543 */
}

int main(void)
{
    /* -- 변수 선언 -------------------------------------------------------- */
    SensorRecord   records[MAX_SAMPLES];
    CircularBuffer cb;
    Statistics     stats;

    int i;
    int sample_count = 0;
    int raw_value    = 0;

    /* -- 난수 시드 초기화 ------------------------------------------------- */
    srand((unsigned int)time(NULL));

    /* -- 구조체 초기화 ---------------------------------------------------- */
    init_circular_buffer(&cb);
    for (i = 0; i < MAX_SAMPLES; i++)
        init_sensor_record(&records[i], 1);

    /* -- 시작 안내 -------------------------------------------------------- */
    printf("\n");
    printf("  +------------------------------------------------------+\n");
    printf("  |   ADC 이동 평균 필터 시뮬레이터                     |\n");
    printf("  |   [신호 모델] 고정 범위 입력                        |\n");
    printf("  +------------------------------------------------------+\n");
    printf("  |   - 입력 범위 : %d ~ %d (범위 폭: %d)          |\n",
        INPUT_MIN, INPUT_MAX, INPUT_RANGE);
    printf("  |   - 필터 크기 : %d샘플 이동 평균                 |\n", FILTER_SIZE);
    printf("  |   - 총 샘플   : %d개                             |\n", MAX_SAMPLES);
    printf("  +------------------------------------------------------+\n");

    printf("\n  [상태 레지스터 비트 구조 안내]\n");
    printf("  Bit7=ACTIVE | Bit6=FILTER_RDY | Bit5=OVER_ERR | Bit4=UNDER_ERR\n");
    printf("  Bit3=VALID  | Bit2=BUF_FULL   | Bit1=(예약)   | Bit0=INIT_DONE\n");

    print_header();

    /* -- 메인 처리 루프 --------------------------------------------------- */
    while (sample_count < MAX_SAMPLES)
    {
        raw_value = generate_range_value();  /* 482 ~ 543 범위 난수 */

        printf("  [#%3d] 입력값: %4d\n", sample_count + 1, raw_value);

        process_sample(records, sample_count, &cb, raw_value);

        print_record(&records[sample_count], sample_count);
        print_bit_pattern(records[sample_count].status_flags);
        print_buffer_state(&cb);

        sample_count++;

        if (sample_count == FILTER_SIZE)
            printf("\n  [*] 필터 버퍼 포화 완료! 이제부터 완전한 이동 평균 적용 [*]\n\n");
    }

    printf("+--------------------------------------------------------------+\n");
    printf("\n  총 %d개의 샘플 처리 완료.\n", sample_count);

    /* -- 통계 분석 -------------------------------------------------------- */
    compute_statistics(records, sample_count, &stats);
    print_statistics(&stats);

    /* -- 파일 저장 -------------------------------------------------------- */
    printf("\n  [파일 저장 중...]\n");
    printf("  저장 경로: ./ (현재 실행 디렉터리)\n\n");

    if (save_report_txt(records, sample_count, &stats))
        printf("  [OK] 보고서 저장 완료 -> ./%s\n", REPORT_FILENAME);
    else
        printf("  [FAIL] 보고서 저장 실패\n");

    if (save_data_csv(records, sample_count))
        printf("  [OK] CSV 저장 완료   -> ./%s\n", CSV_FILENAME);
    else
        printf("  [FAIL] CSV 저장 실패\n");

    printf("\n  프로그램을 종료합니다.\n\n");
    return 0;
}
