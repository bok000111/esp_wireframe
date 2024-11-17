
# esp_wireframe

ESP32-C3와 FreeRTOS 환경에서 LVGL을 활용하여 간단한 와이어프레임 렌더링과 실시간 메모리 사용량 모니터링을 구현한 프로젝트입니다.

## 주요 기능

- **3D 정점 변환 및 투영**: 기본적인 기하학 연산을 통해 3D 공간의 정점을 변환하고, 이를 2D 화면에 투영하여 와이어프레임 객체를 렌더링합니다.
- **LVGL 타이머와 FreeRTOS 하드웨어 타이머 연동**: LVGL의 주기적인 화면 갱신을 FreeRTOS 하드웨어 타이머와 연동하여 정확한 화면 갱신을 구현합니다.
- **메모리 사용량 모니터링**: FreeRTOS의 메모리 관리 API를 활용하여 스택 및 힙 메모리 사용량을 실시간으로 측정하고, LVGL을 통해 GUI로 표시합니다.
- **멀티태스킹**: 와이어프레임 렌더링과 메모리 모니터링을 각각 독립적인 FreeRTOS 태스크로 분리하여 실행합니다.

## 요구 사항

- ESP32-C3 모듈
- ESP-IDF v5.3.1 이상
- LVGL 라이브러리

## 설치 및 빌드

1. ESP-IDF 환경을 설정합니다. 자세한 내용은 [ESP-IDF 공식 문서](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html)를 참고하세요.
2. 이 저장소를 클론합니다:

   ```bash
   git clone https://github.com/bok000111/esp_wireframe.git
   ```

3. 프로젝트 디렉토리로 이동합니다:

   ```bash
   cd esp_wireframe
   ```

4. 필요한 서브모듈을 초기화하고 업데이트합니다:

   ```bash
   git submodule update --init --recursive
   ```

5. 프로젝트를 빌드하고 플래시합니다:

   ```bash
   idf.py build
   idf.py flash
   ```

6. 시리얼 모니터를 통해 장치의 출력을 확인합니다:

   ```bash
   idf.py monitor
   ```

## 사용 방법

프로그램이 실행되면, LVGL을 통해 와이어프레임 렌더링과 실시간 메모리 사용량 모니터링이 화면에 표시됩니다.
