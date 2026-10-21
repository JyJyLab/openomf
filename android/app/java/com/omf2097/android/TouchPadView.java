package com.omf2097.android;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import org.libsdl.app.SDLActivity;

import java.util.HashSet;
import java.util.Set;

/**
 * 멀티터치 가상 패드. 왼쪽 아래 8방향 패드(↑↓←→ 와 대각선), 오른쪽 아래 PUNCH(Enter)·KICK(오른쪽 Shift),
 * 왼쪽 위 MENU(ESC). 손가락마다 어느 버튼 위에 있는지 계산해, 눌림이 바뀐 키만 SDL 로 보낸다.
 */
public class TouchPadView extends View {
    private static final int UP = KeyEvent.KEYCODE_DPAD_UP, DOWN = KeyEvent.KEYCODE_DPAD_DOWN;
    private static final int LEFT = KeyEvent.KEYCODE_DPAD_LEFT, RIGHT = KeyEvent.KEYCODE_DPAD_RIGHT;
    private static final int PUNCH = KeyEvent.KEYCODE_ENTER, KICK = KeyEvent.KEYCODE_SHIFT_RIGHT;
    private static final int MENU = KeyEvent.KEYCODE_ESCAPE;

    private final Set<Integer> down = new HashSet<>();
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG), stroke = new Paint(Paint.ANTI_ALIAS_FLAG),
            text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private boolean hidden = false;  // 게임패드 연결 등으로 방향·공격 버튼을 숨김 (톱니 버튼은 항상)

    // 화면 크기에 따라 계산하는 버튼 위치
    private float padX, padY, padR, punchX, punchY, kickX, kickY, btnR, menuX, menuY, menuR, gearX, gearY, gearR;

    public TouchPadView(Context c) {
        super(c);
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeWidth(4f);
        stroke.setColor(Color.argb(150, 255, 255, 255));
        text.setColor(Color.argb(190, 255, 255, 255));
        text.setTextAlign(Paint.Align.CENTER);
        text.setFakeBoldText(true);
    }

    void setPadVisible(boolean show) {
        if (hidden == show) {
            hidden = !show;
            releaseAll();
            postInvalidate();
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        float u = Math.min(w, h);
        padR = u * 0.22f;
        padX = padR * 1.25f;
        padY = h - padR * 1.25f;
        btnR = u * 0.11f;
        kickX = w - btnR * 1.4f;
        kickY = h - btnR * 1.6f;
        punchX = kickX - btnR * 2.4f;
        punchY = kickY + btnR * 0.3f;
        menuR = u * 0.06f;
        menuX = menuR * 1.6f;
        menuY = menuR * 1.6f;
        gearR = u * 0.045f;
        gearX = w - gearR * 1.8f;
        gearY = gearR * 1.8f;
        text.setTextSize(btnR * 0.42f);
    }

    @Override
    protected void onDraw(Canvas c) {
        // 오버레이 옵션 (톱니) — 항상 표시
        fill.setColor(Color.argb(60, 200, 200, 200));
        c.drawCircle(gearX, gearY, gearR, fill);
        c.drawCircle(gearX, gearY, gearR, stroke);
        float tsz = text.getTextSize();
        text.setTextSize(gearR * 1.2f);
        c.drawText("\u2699", gearX, gearY + gearR * 0.42f, text);
        text.setTextSize(tsz);
        if (hidden) return;
        // 방향 패드
        fill.setColor(Color.argb(60, 255, 255, 255));
        c.drawCircle(padX, padY, padR, fill);
        c.drawCircle(padX, padY, padR, stroke);
        fill.setColor(Color.argb(110, 255, 255, 255));
        float a = padR * 0.62f, s = padR * 0.16f;
        drawArrow(c, padX, padY - a, s, 0, down.contains(UP));
        drawArrow(c, padX, padY + a, s, 180, down.contains(DOWN));
        drawArrow(c, padX - a, padY, s, 270, down.contains(LEFT));
        drawArrow(c, padX + a, padY, s, 90, down.contains(RIGHT));
        // 공격 버튼
        button(c, punchX, punchY, btnR, "PUNCH", down.contains(PUNCH), Color.argb(90, 230, 60, 60));
        button(c, kickX, kickY, btnR, "KICK", down.contains(KICK), Color.argb(90, 60, 120, 230));
        text.setTextSize(menuR * 0.55f);
        button(c, menuX, menuY, menuR, "MENU", down.contains(MENU), Color.argb(70, 200, 200, 200));
        text.setTextSize(btnR * 0.42f);
    }

    private void drawArrow(Canvas c, float x, float y, float s, float deg, boolean on) {
        fill.setColor(on ? Color.argb(220, 255, 220, 80) : Color.argb(130, 255, 255, 255));
        c.save();
        c.rotate(deg, x, y);
        android.graphics.Path p = new android.graphics.Path();
        p.moveTo(x, y - s);
        p.lineTo(x - s, y + s * 0.6f);
        p.lineTo(x + s, y + s * 0.6f);
        p.close();
        c.drawPath(p, fill);
        c.restore();
    }

    private void button(Canvas c, float x, float y, float r, String label, boolean on, int color) {
        fill.setColor(on ? Color.argb(200, 255, 220, 80) : color);
        c.drawCircle(x, y, r, fill);
        c.drawCircle(x, y, r, stroke);
        c.drawText(label, x, y + text.getTextSize() * 0.35f, text);
    }

    /** 한 손가락 위치가 누르는 키들 */
    private void keysAt(float x, float y, Set<Integer> out) {
        float dx = x - padX, dy = y - padY;
        double d = Math.hypot(dx, dy);
        if (d < padR * 1.35 && d > padR * 0.18) {
            // 8방향: 45도 구간마다 (대각선은 두 키)
            double ang = Math.toDegrees(Math.atan2(dy, dx));  // 0=오른쪽, 90=아래
            if (ang > -67.5 && ang < 67.5) out.add(RIGHT);
            if (ang > 22.5 && ang < 157.5) out.add(DOWN);
            if (ang > 112.5 || ang < -112.5) out.add(LEFT);
            if (ang > -157.5 && ang < -22.5) out.add(UP);
            return;
        }
        if (Math.hypot(x - punchX, y - punchY) < btnR * 1.25) out.add(PUNCH);
        if (Math.hypot(x - kickX, y - kickY) < btnR * 1.25) out.add(KICK);
        if (Math.hypot(x - menuX, y - menuY) < menuR * 1.5) out.add(MENU);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (e.getActionMasked() == MotionEvent.ACTION_DOWN && Math.hypot(e.getX() - gearX, e.getY() - gearY) < gearR * 1.6) {
            ((OMFActivity) getContext()).showOverlayOptions();
            return true;
        }
        if (hidden) return false;
        Set<Integer> now = new HashSet<>();
        int action = e.getActionMasked();
        int lifted = (action == MotionEvent.ACTION_POINTER_UP) ? e.getActionIndex() : -1;
        if (action != MotionEvent.ACTION_UP && action != MotionEvent.ACTION_CANCEL) {
            for (int i = 0; i < e.getPointerCount(); i++) {
                if (i != lifted) keysAt(e.getX(i), e.getY(i), now);
            }
        }
        boolean consumed = false;
        for (Integer k : new HashSet<>(down)) {
            if (!now.contains(k)) {
                SDLActivity.onNativeKeyUp(k);
                down.remove(k);
            }
        }
        for (Integer k : now) {
            if (!down.contains(k)) {
                SDLActivity.onNativeKeyDown(k);
                down.add(k);
            }
        }
        consumed = !now.isEmpty() || action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP
                || action == MotionEvent.ACTION_MOVE;
        invalidate();
        // 버튼이 아닌 곳의 터치는 SDL 로 넘긴다 (메뉴의 마우스 대용 등)
        return consumed && !(now.isEmpty() && action == MotionEvent.ACTION_DOWN);
    }

    private void releaseAll() {
        for (Integer k : down) SDLActivity.onNativeKeyUp(k);
        down.clear();
    }
}
