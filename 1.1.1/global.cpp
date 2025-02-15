#include "GLOBAL.h"

// 측정 상태 플래그 변수들 정의
bool isMeasuring = false;  // 초기값은 false
bool isSaving = false;     // 초기값은 false
bool isSDbusy = false;     // 초기값은 false
bool isUpdating = false;   // 초기값은 false

// 현재 파일 이름을 저장할 배열 정의
String currentlysavingFile = "";  // 빈 문자열로 초기화