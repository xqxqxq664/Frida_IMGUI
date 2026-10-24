package com.example.application3;

import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import javax.microedition.khronos.opengles.GL10;
import javax.microedition.khronos.egl.EGLConfig;

public class MyRenderer implements GLSurfaceView.Renderer {

    public static native void nativeOnSurfaceCreated();
    public static native void nativeOnSurfaceChanged(int width, int height);
    public static native void nativeOnDrawFrame();
    public static native boolean nativeOnTouch(int action, float x, float y);
    public static native void nativeOnImeText(String text);
    public static native void nativeOnKeyboardClosed();

    private static GLSurfaceView g_renderView = null;
    private static View g_touchView = null;
    private static View g_gameContent = null;
    private static ViewGroup g_root = null;

    private static android.app.Activity g_activity = null;
    private static EditText g_hiddenEdit = null;

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        nativeOnSurfaceCreated();
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        nativeOnSurfaceChanged(width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        nativeOnDrawFrame();
    }

    public static android.app.Activity getActivity() {
        try {
            Class<?> activityThreadCls = Class.forName("android.app.ActivityThread");
            java.lang.reflect.Method currentMethod = activityThreadCls.getMethod("currentActivityThread");
            Object activityThread = currentMethod.invoke(null);

            java.lang.reflect.Field mActivitiesField = activityThreadCls.getDeclaredField("mActivities");
            mActivitiesField.setAccessible(true);
            java.util.Map<?, ?> activities = (java.util.Map<?, ?>) mActivitiesField.get(activityThread);

            for (Object record : activities.values()) {
                Class<?> recordCls = record.getClass();
                java.lang.reflect.Field activityField = recordCls.getDeclaredField("activity");
                activityField.setAccessible(true);
                return (android.app.Activity) activityField.get(record);
            }
        } catch (Throwable e) {
        }
        return null;
    }

    public static void keepOnTop() {
        if (g_root == null) return;
        g_root.post(new Runnable() {
				@Override
				public void run() {
					try {
						if (g_renderView != null) g_renderView.bringToFront();
						if (g_touchView  != null) g_touchView.bringToFront();
					} catch (Throwable t) {}
				}
			});
    }

    public static void showKeyboard(final String initialText) {
        if (g_activity == null) return;
        g_activity.runOnUiThread(new Runnable() {
				@Override
				public void run() {
					try {
						if (g_hiddenEdit == null) {
							g_hiddenEdit = new EditText(g_activity);
							g_hiddenEdit.setAlpha(0.01f);
							g_hiddenEdit.setBackgroundColor(0x00000000);

							g_hiddenEdit.addTextChangedListener(new TextWatcher() {
									@Override public void beforeTextChanged(CharSequence s, int st, int c, int a) {}
									@Override public void onTextChanged(CharSequence s, int st, int b, int c) {}
									@Override public void afterTextChanged(Editable s) {
										nativeOnImeText(s.toString());
									}
								});

							ViewGroup root = (ViewGroup) g_activity.getWindow().getDecorView();
							root.addView(g_hiddenEdit, new ViewGroup.LayoutParams(1, 1));
						}

						g_hiddenEdit.setText(initialText);
						g_hiddenEdit.setSelection(initialText.length());
						g_hiddenEdit.requestFocus();

						InputMethodManager imm = (InputMethodManager)
							g_activity.getSystemService(Context.INPUT_METHOD_SERVICE);
						imm.showSoftInput(g_hiddenEdit, InputMethodManager.SHOW_IMPLICIT);
					} catch (Throwable t) {
					}
				}
			});
    }

    public static void hideKeyboard() {
        if (g_activity == null) return;
        g_activity.runOnUiThread(new Runnable() {
				@Override
				public void run() {
					try {
						if (g_hiddenEdit != null) {
							InputMethodManager imm = (InputMethodManager)
								g_activity.getSystemService(Context.INPUT_METHOD_SERVICE);
							imm.hideSoftInputFromWindow(g_hiddenEdit.getWindowToken(), 0);
						}
						nativeOnKeyboardClosed();
					} catch (Throwable t) {
					}
				}
			});
    }

    private static boolean forwardToGame(MotionEvent event) {
        if (g_gameContent == null) return false;
        try {
            MotionEvent copy = MotionEvent.obtain(event);
            g_gameContent.dispatchTouchEvent(copy);
            copy.recycle();
        } catch (Throwable t) {
        }
        return true;
    }

    public static void createAndAttach(final android.app.Activity act) {
        act.runOnUiThread(new Runnable() {
				@Override
				public void run() {
					try {
						g_activity = act;
						g_gameContent = act.findViewById(android.R.id.content);
						g_root = (ViewGroup) act.getWindow().getDecorView();

						GLSurfaceView glsv = new GLSurfaceView(act);
						glsv.setEGLContextClientVersion(3);
						glsv.setZOrderOnTop(true);
						glsv.getHolder().setFormat(PixelFormat.TRANSLUCENT);
						glsv.setEGLConfigChooser(8, 8, 8, 8, 16, 0);
						glsv.setRenderer(new MyRenderer());
						glsv.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);

						View touch = new View(act);
						touch.setBackgroundColor(0x00000000);
						touch.setOnTouchListener(new View.OnTouchListener() {
								@Override
								public boolean onTouch(View v, MotionEvent event) {
									boolean want = nativeOnTouch(
										event.getActionMasked(),
										event.getX(),
										event.getY()
									);
									if (want) return true;
									return forwardToGame(event);
								}
							});

						g_root.addView(glsv, new ViewGroup.LayoutParams(
										   ViewGroup.LayoutParams.MATCH_PARENT,
										   ViewGroup.LayoutParams.MATCH_PARENT
									   ));
						g_root.addView(touch, new ViewGroup.LayoutParams(
										   ViewGroup.LayoutParams.MATCH_PARENT,
										   ViewGroup.LayoutParams.MATCH_PARENT
									   ));

						g_renderView = glsv;
						g_touchView = touch;
					} catch (Throwable t) {
					}
				}
			});
    }
}