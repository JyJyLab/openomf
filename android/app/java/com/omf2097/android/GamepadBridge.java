package com.omf2097.android;

import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.libsdl.app.SDLActivity;

/**
 * 블루투스·유선 게임패드 입력을 OpenOMF 1P 키보드 키로 바꿔 보낸다 (게임 안 설정 불필요).
 *   방향(D-pad·왼쪽 스틱) → 방향키, 펀치 → Enter, 킥 → 오른쪽 Shift, START·SELECT → ESC
 *   버튼 이름 기준: Xbox 계열 A=펀치·B=킥, 닌텐도(스위치) 계열 B=펀치·A=킥.
 *   (기기마다 다르게 들어오면 오버레이 옵션의 "펀치/킥 바꾸기" 로 뒤집는다)
 */
final class GamepadBridge {
    private static final int UP = KeyEvent.KEYCODE_DPAD_UP, DOWN = KeyEvent.KEYCODE_DPAD_DOWN;
    private static final int LEFT = KeyEvent.KEYCODE_DPAD_LEFT, RIGHT = KeyEvent.KEYCODE_DPAD_RIGHT;
    static final int PUNCH = KeyEvent.KEYCODE_ENTER, KICK = KeyEvent.KEYCODE_SHIFT_RIGHT, ESC = KeyEvent.KEYCODE_ESCAPE;
    private static final int NINTENDO_VENDOR = 0x057e;

    private boolean stickUp, stickDown, stickLeft, stickRight;
    boolean swapPunchKick = false;  // 오버레이 옵션

    static boolean isGamepad(InputDevice d) {
        if (d == null || d.isVirtual()) return false;
        int s = d.getSources();
        return (s & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                || (s & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }

    static boolean fromGamepad(int source) {
        return (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                || (source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
                || (source & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD;
    }

    /** 이 키를 처리했으면 true (SDL 로는 보내지 않음) */
    boolean onKey(KeyEvent e) {
        if (!fromGamepad(e.getSource())) return false;
        InputDevice dev = e.getDevice();
        boolean nintendo = dev != null && dev.getVendorId() == NINTENDO_VENDOR;
        int out;
        switch (e.getKeyCode()) {
            case KeyEvent.KEYCODE_DPAD_UP: out = UP; break;
            case KeyEvent.KEYCODE_DPAD_DOWN: out = DOWN; break;
            case KeyEvent.KEYCODE_DPAD_LEFT: out = LEFT; break;
            case KeyEvent.KEYCODE_DPAD_RIGHT: out = RIGHT; break;
            case KeyEvent.KEYCODE_BUTTON_A: out = nintendo ? KICK : PUNCH; break;
            case KeyEvent.KEYCODE_BUTTON_B: out = nintendo ? PUNCH : KICK; break;
            case KeyEvent.KEYCODE_BUTTON_X: out = nintendo ? KICK : PUNCH; break;
            case KeyEvent.KEYCODE_BUTTON_Y: out = nintendo ? PUNCH : KICK; break;
            case KeyEvent.KEYCODE_BUTTON_START:
            case KeyEvent.KEYCODE_BUTTON_SELECT:
            case KeyEvent.KEYCODE_BUTTON_MODE: out = ESC; break;
            case KeyEvent.KEYCODE_DPAD_CENTER: out = PUNCH; break;
            default: return false;
        }
        if (swapPunchKick && (out == PUNCH || out == KICK)) out = out == PUNCH ? KICK : PUNCH;
        if (e.getAction() == KeyEvent.ACTION_DOWN) {
            if (e.getRepeatCount() == 0) SDLActivity.onNativeKeyDown(out);
        } else if (e.getAction() == KeyEvent.ACTION_UP) {
            SDLActivity.onNativeKeyUp(out);
        }
        return true;
    }

    /** 왼쪽 스틱·HAT 을 방향키로 */
    boolean onMotion(MotionEvent e) {
        if ((e.getSource() & InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK
                || e.getActionMasked() != MotionEvent.ACTION_MOVE) return false;
        float x = e.getAxisValue(MotionEvent.AXIS_X), y = e.getAxisValue(MotionEvent.AXIS_Y);
        float hx = e.getAxisValue(MotionEvent.AXIS_HAT_X), hy = e.getAxisValue(MotionEvent.AXIS_HAT_Y);
        if (Math.abs(hx) > 0.5f) x = hx;
        if (Math.abs(hy) > 0.5f) y = hy;
        stickUp = set(stickUp, y < -0.5f, UP);
        stickDown = set(stickDown, y > 0.5f, DOWN);
        stickLeft = set(stickLeft, x < -0.5f, LEFT);
        stickRight = set(stickRight, x > 0.5f, RIGHT);
        return true;
    }

    private static boolean set(boolean was, boolean now, int key) {
        if (now && !was) SDLActivity.onNativeKeyDown(key);
        if (!now && was) SDLActivity.onNativeKeyUp(key);
        return now;
    }
}
