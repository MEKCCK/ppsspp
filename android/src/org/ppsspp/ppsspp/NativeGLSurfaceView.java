package org.ppsspp.ppsspp;

// Touch-enabled GLSurfaceView.
// Used when javaGL = true.

import android.annotation.SuppressLint;
import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

public class NativeGLSurfaceView extends GLSurfaceView {
	public NativeGLSurfaceView(Context context) {
		super(context);
		// The IME can only connect to a focusable view. Without this, showSoftInput() has
		// nothing to attach to (see NativeTextInput).
		setFocusable(true);
		setFocusableInTouchMode(true);
	}

	@Override
	public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
		return NativeTextInput.createInputConnection(this, outAttrs);
	}

	@SuppressLint("ClickableViewAccessibility")
	@Override
	public boolean onTouchEvent(final MotionEvent ev) {
		NativeApp.processTouchEvent(ev);
		return true;
	}
}
