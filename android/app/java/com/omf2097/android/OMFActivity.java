package com.omf2097.android;

import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.hardware.input.InputManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewGroup;

import org.libsdl.app.SDLActivity;

/**
 * One Must Fall 2097 (OpenOMF).
 * SDL 화면 위에 터치 가상 패드와 오버레이 옵션(톱니) 버튼을 올린다.
 * 게임패드가 연결되면 가상 패드는 자동으로 숨고, 게임패드 입력은 1P 키보드 키로 바뀌어 들어간다.
 */
public class OMFActivity extends SDLActivity implements InputManager.InputDeviceListener {
    static final String PREFS = "omf_overlay";
    static final String PAD_MODE = "pad_mode";      // 0 자동(게임패드 있으면 숨김), 1 항상 표시, 2 항상 숨김
    static final String SWAP = "swap_punch_kick";
    static final String EASY_FINISH = "easy_finish";  // 간편 마무리 (기본 켜짐)

    /** 엔진(main.c): 승리 후 주먹 = 반파, 반파 중 발 = 완파 */
    static native void nativeSetEasyFinish(boolean on);

    private TouchPadView pad;
    private final GamepadBridge bridge = new GamepadBridge();
    private InputManager inputManager;
    private SharedPreferences prefs;

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "main"};
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        prefs = getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        bridge.swapPunchKick = prefs.getBoolean(SWAP, false);
        setEasyFinish(prefs.getBoolean(EASY_FINISH, true));
        if (mLayout != null) {
            pad = new TouchPadView(this);
            mLayout.addView(pad, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        }
        inputManager = (InputManager) getSystemService(Context.INPUT_SERVICE);
        if (inputManager != null) inputManager.registerInputDeviceListener(this, new Handler(Looper.getMainLooper()));
        updatePadVisibility();
    }

    @Override
    protected void onDestroy() {
        if (inputManager != null) inputManager.unregisterInputDeviceListener(this);
        super.onDestroy();
        // 게임이 끝나 화면이 닫히면 프로세스까지 끝낸다.
        // 안드로이드는 프로세스와 libmain.so 를 남겨 두는데, 그 상태로 다시 실행하면
        // OpenOMF 의 전역 상태(그래픽 바인딩 등)가 초기화되지 않아 검은 화면이 된다.
        if (isFinishing()) {
            System.exit(0);
        }
    }

    static boolean gamepadConnected() {
        for (int id : InputDevice.getDeviceIds()) {
            if (GamepadBridge.isGamepad(InputDevice.getDevice(id))) return true;
        }
        return false;
    }

    void updatePadVisibility() {
        if (pad == null) return;
        int mode = prefs.getInt(PAD_MODE, 0);
        boolean show = mode == 1 || (mode == 0 && !gamepadConnected());
        pad.setPadVisible(show);
    }

    @Override public void onInputDeviceAdded(int id) { updatePadVisibility(); }
    @Override public void onInputDeviceRemoved(int id) { updatePadVisibility(); }
    @Override public void onInputDeviceChanged(int id) { updatePadVisibility(); }

    private static void setEasyFinish(boolean on) {
        try {
            nativeSetEasyFinish(on);
        } catch (UnsatisfiedLinkError e) {
            // 라이브러리를 못 불러온 경우(SDL 이 오류 화면을 띄움) — 무시
        }
    }

    /** 오버레이 옵션 (톱니 버튼) */
    void showOverlayOptions() {
        final String[] modes = {"자동 (게임패드 연결 시 숨김)", "항상 표시", "항상 숨김"};
        final boolean swap = prefs.getBoolean(SWAP, false);
        final boolean easy = prefs.getBoolean(EASY_FINISH, true);
        String[] items = {
            "가상 패드: " + modes[prefs.getInt(PAD_MODE, 0)],
            "게임패드 펀치/킥 바꾸기: " + (swap ? "켜짐" : "꺼짐"),
            "간편 마무리 (승리 후 주먹=반파, 발=완파): " + (easy ? "켜짐" : "꺼짐"),
            "닫기",
        };
        new AlertDialog.Builder(this)
            .setTitle("오버레이 옵션")
            .setItems(items, (d, which) -> {
                if (which == 0) {
                    prefs.edit().putInt(PAD_MODE, (prefs.getInt(PAD_MODE, 0) + 1) % 3).apply();
                    updatePadVisibility();
                    showOverlayOptions();
                } else if (which == 1) {
                    prefs.edit().putBoolean(SWAP, !swap).apply();
                    bridge.swapPunchKick = !swap;
                    showOverlayOptions();
                } else if (which == 2) {
                    prefs.edit().putBoolean(EASY_FINISH, !easy).apply();
                    setEasyFinish(!easy);
                    showOverlayOptions();
                }
            })
            .show();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        // 뒤로 가기 버튼 = ESC (게임 메뉴·일시정지)
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK && !GamepadBridge.fromGamepad(event.getSource())) {
            if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
                SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_ESCAPE);
            } else if (event.getAction() == KeyEvent.ACTION_UP) {
                SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_ESCAPE);
            }
            return true;
        }
        if (bridge.onKey(event)) return true;
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (bridge.onMotion(event)) return true;
        return super.dispatchGenericMotionEvent(event);
    }
}
