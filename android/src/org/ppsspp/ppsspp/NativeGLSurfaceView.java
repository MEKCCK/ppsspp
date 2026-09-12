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

	@Override
	public boolean onCheckIsTextEditor() {
		// Without this the IME considers this view not to be a text editor at all and refuses to
		// connect to it, no matter that it has an input connection.
		return NativeTextInput.isTextInputActive();
	}

	@SuppressLint("ClickableViewAccessibility")
	@Override
	public boolean onTouchEvent(final MotionEvent ev) {
		NativeApp.processTouchEvent(ev);
		return true;
	}
}
