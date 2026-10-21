# OpenOMF for Android (unofficial fork)

[한국어](#한국어) · [English](#english)

Native Android port of [OpenOMF](https://github.com/omf2097/openomf) — the open-source remake of One Must Fall 2097.
No DOS emulator: the OpenOMF engine runs natively with SDL2 + OpenGL ES 3.0.

This is a personal, unofficial fork. **No game data is included.** You need your own copy of the original
One Must Fall 2097 files (released as freeware by Diversions Entertainment).

---

## English

### What changed compared to upstream OpenOMF
- **Renderer**: OpenGL 3.3 core → OpenGL ES 3.0 (`android/shim/epoxy/gl.h` replaces libepoxy; the paletted
  framebuffer uses `GL_RGBA16F` because ES has no renderable `GL_RGBA16`; `glMultiDrawArrays` is emulated).
- **Shaders** are written so that they compile both as GLSL 330 and GLSL ES 3.00; on Android the `#version`
  line is swapped at load time.
- **Assets**: game data is packed into the APK (`assets/omf`) and copied to the app's private folder on first run.
  Log, config and saves are in `Android/data/com.omf2097.android/files/`.
- **Lifecycle**: GL objects are rebuilt when SDL recreates the GL context after the app was in the background;
  the process exits when the game quits (a relaunch then starts clean).
- **Text input**: the on-screen keyboard opens while a name field is selected; its Enter key is held briefly so
  the polled keyboard controller sees it.
- **Controls** (`android/app/java/com/omf2097/android`):
  - touch pad: 8-way D-pad, PUNCH (= Enter), KICK (= Right Shift), MENU (= Esc); multitouch
  - gamepads (Bluetooth/USB) are translated to the same keyboard keys, the touch pad hides automatically.
    Xbox-style A = punch, B = kick; Nintendo-style B = punch, A = kick
  - overlay options (gear button): touch pad auto/always/never, swap punch/kick
- Networking features are present in the code but not used on Android.

### Building (Linux)
Requirements: JDK 17+, CMake, Ninja, Android SDK (`platforms;android-34`, `build-tools;34.0.0`, `cmdline-tools`),
Android NDK (tested r27c), and a desktop OpenOMF build for the generated resources.

```sh
# 1. desktop build (creates build-linux/resources: openomf.bk, *.LNG2, *.DAT2 ...)
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build-linux

# 2. Android dependencies -> android/work/prefix
export ANDROID_HOME=/path/to/android-sdk ANDROID_NDK_HOME=/path/to/android-ndk-r27c
android/build_deps.sh

# 3. APK (arm64-v8a, Android 7.0+)
OMF_DATA=/path/to/OMF2097 android/build_apk.sh 0.6
# -> android/work/out/OpenOMF-0.6.apk (signed with a personal key created in android/work)
```

---

## 한국어

### 원본 OpenOMF 와 다른 점
- **그래픽**: 데스크톱 OpenGL 3.3 → 안드로이드 OpenGL ES 3.0. 팔레트 프레임버퍼는 GLES 에서 그릴 수 있는 `GL_RGBA16F`(half float) 사용.
- **셰이더**: GLSL 330 과 GLSL ES 3.00 양쪽에서 컴파일되도록 수정, 안드로이드에서는 읽을 때 `#version` 줄만 바꿈.
- **게임 데이터**: APK 안(`assets/omf`)에 넣고 처음 실행할 때 앱 폴더로 복사. 로그·설정·세이브는 `Android/data/com.omf2097.android/files/`.
- **백그라운드 복귀**: GL 컨텍스트가 새로 만들어지면 GL 객체를 다시 만듦. 게임을 끝내면 프로세스도 종료해 다시 실행할 때 깨끗하게 시작.
- **이름 입력**: 입력 칸이 선택되면 화면 키보드 표시, 키보드 Enter 로 확정.
- **조작**: 터치 가상 패드(8방향, PUNCH=Enter, KICK=오른쪽 Shift, MENU=Esc, 멀티터치), 블루투스·유선 게임패드는 같은 키로 변환되고 연결되면 터치 패드 자동 숨김
  (Xbox 계열 A 펀치·B 킥, 닌텐도 계열 B 펀치·A 킥). 톱니 버튼의 오버레이 옵션: 터치 패드 자동/항상/숨김, 펀치·킥 바꾸기.

### 빌드
위 English 의 Building 순서와 같습니다. 원본 게임 파일 폴더를 `OMF_DATA` 로 지정해야 합니다(저장소에는 게임 데이터가 없음).

---

License: same as OpenOMF (MIT, see `LICENSE`). SDL2 Java sources in `android/app/java/org/libsdl` are under the zlib license.
