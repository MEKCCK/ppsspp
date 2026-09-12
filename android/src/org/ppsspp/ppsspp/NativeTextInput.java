package org.ppsspp.ppsspp;

import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

// The text input target for PPSSPP's own text fields on Android - the equivalent of the
// UIKeyInput implementation iOS has (ios/ViewControllerCommon.mm) and of ActivateTextEditInput
// on UWP. The shared UI calls System_NotifyUIEvent(TEXT_GOTFOCUS) when a text field is focused,
// which makes the activity show the keyboard on the surface view; this class turns whatever the
// IME commits into the same keyChar events the rest of PPSSPP already understands (compare
// SendKeyboardChars in ios/Controls.mm).
//
// Without it the surface view has no input connection at all, so showSoftInput() has nothing to
// connect to and text (including anything pasted from, or composed by, an IME) is silently lost.
public final class NativeTextInput {
	private NativeTextInput() {
	}

	public static InputConnection createInputConnection(View target, EditorInfo outAttrs) {
		outAttrs.inputType = InputType.TYPE_CLASS_TEXT
				| InputType.TYPE_TEXT_FLAG_MULTI_LINE
				| InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
		// Let ENTER arrive as a key event rather than being turned into an editor action, so the
		// editor's normal newline handling applies.
		outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_ENTER_ACTION;
		return new NativeInputConnection(target);
	}

	private static final class NativeInputConnection extends BaseInputConnection {
		NativeInputConnection(View target) {
			super(target, false);
		}

		@Override
		public boolean commitText(CharSequence text, int newCursorPosition) {
			if (text == null) {
				return true;
			}
			// One keyChar per code point, which is exactly what the native side expects.
			for (int i = 0; i < text.length(); ) {
				final int codePoint = Character.codePointAt(text, i);
				NativeApp.keyChar(NativeApp.DEVICE_ID_KEYBOARD, codePoint);
				i += Character.charCount(codePoint);
			}
			return true;
		}

		@Override
		public boolean setComposingText(CharSequence text, int newCursorPosition) {
			// Composing text (pinyin, kana, ...) must not reach the editor; commitText delivers
			// the finished characters. iOS behaves the same way.
			return true;
		}

		@Override
		public boolean finishComposingText() {
			return true;
		}

		@Override
		public boolean deleteSurroundingText(int beforeLength, int afterLength) {
			if (afterLength > 0) {
				// Deleting forward is not something the editor needs from an IME; ignore it
				// rather than removing text the user cannot see changing.
				return true;
			}
			return backspace(beforeLength);
		}

		@Override
		public boolean deleteSurroundingTextInCodePoints(int beforeLength, int afterLength) {
			// Newer keyboards call this one instead; in code points, which is also one key press
			// per character for us.
			if (afterLength > 0) {
				return true;
			}
			return backspace(beforeLength);
		}

		// What iOS does in deleteBackward: one backspace per character.
		private boolean backspace(int count) {
			for (int i = 0; i < count; i++) {
				NativeApp.keyDown(NativeApp.DEVICE_ID_KEYBOARD, KeyEvent.KEYCODE_DEL, false);
				NativeApp.keyUp(NativeApp.DEVICE_ID_KEYBOARD, KeyEvent.KEYCODE_DEL);
			}
			return true;
		}

		@Override
		public boolean sendKeyEvent(KeyEvent event) {
			// Enter, arrows and so on go through the view's normal key handling, which already
			// forwards them to native code.
			return super.sendKeyEvent(event);
		}
	}
}
