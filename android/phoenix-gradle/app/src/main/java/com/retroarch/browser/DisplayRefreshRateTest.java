package com.retroarch.browser;

import android.app.Activity;
import android.content.SharedPreferences;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import com.retroarch.browser.preferences.fragments.AudioVideoPreferenceFragment;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarchlite.R;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * {@link Activity} subclass that provides the functionality
 * for the refresh rate testing for device displays.
 */
public final class DisplayRefreshRateTest extends Activity {

   private double fps = 0.0;

   private class Renderer implements GLSurfaceView.Renderer {
      private static final String TAG = "GLESRenderer";
      private static final double WARMUP_SECONDS = 2.0;
      private static final double TEST_SECONDS = 10.0;
      // Test states
      private static final int STATE_START = 0;
      private static final int STATE_WARMUP = 1;
      private static final int STATE_TEST = 2;
      private static final int STATE_DONE = 3;
      private static final int STATE_DEAD = 4;
      private int mState = STATE_START;
      private double mStartTime = 0.0;
      private int mNumFrames = 0;

      private final Activity activity;

      public Renderer(Activity activity) {
         this.activity = activity;
      }

      private void setFPSSetting() {
         double refreshRate = AudioVideoPreferenceFragment.dividedRefreshRate(fps);
         SharedPreferences prefs = UserPreferences.getPreferences(DisplayRefreshRateTest.this);
         SharedPreferences.Editor edit = prefs.edit();
         edit.putString("video_refresh_rate", Double.toString(refreshRate));
         edit.apply();
      }

      @Override
      public void onDrawFrame(GL10 gl) {
         double t = System.nanoTime() * 1.0e-9;
         switch (mState) {
         case STATE_START:
            mStartTime = t;
            mState = STATE_WARMUP;
            break;

         case STATE_WARMUP:
            if ((t - mStartTime) >= WARMUP_SECONDS) {
               mStartTime = t;
               mNumFrames = 0;
               mState = STATE_TEST;
            }
            break;

         case STATE_TEST:
            mNumFrames++;
            double elapsed = t - mStartTime;
            if (elapsed >= TEST_SECONDS) {
               fps = (double)mNumFrames / elapsed;
               Log.i(TAG, "Measured FPS to: " + fps);
               setFPSSetting();
               mState = STATE_DONE;
            }
            break;

         case STATE_DONE:
            activity.runOnUiThread(DisplayRefreshRateTest.this::finish);
            mState = STATE_DEAD;
            break;
            
         case STATE_DEAD:
            break;
         }

         float luma = (float)Math.sin((double)mNumFrames * 0.10);
         luma *= 0.2f;
         luma += 0.5f;
         GLES20.glClearColor(luma, luma, luma, 1.0f);
         GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      }

      @Override
      public void onSurfaceChanged(GL10 gl, int width, int height) {
         // TODO Auto-generated method stub
      }

      @Override
      public void onSurfaceCreated(GL10 gl, EGLConfig config) {
         // TODO Auto-generated method stub
      }
   }

   @Override
   protected void onCreate(Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);

      DarkToast.makeText(this, R.string.touch_screen_with_fingers);
      final GLSurfaceView surfaceView = new GLSurfaceView(this);
      surfaceView.setEGLContextClientVersion(2);
      surfaceView.setRenderer(new Renderer(this));
      getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
      setTitle(R.string.refresh_rate_calibration);
      setContentView(surfaceView);
   }

   @Override
   protected void onDestroy() {
      SharedPreferences prefs = UserPreferences.getPreferences(this);
      String rate = prefs.getString("video_refresh_rate", "ERROR");
      DarkToast.makeText(this,
            String.format(getString(R.string.refresh_rate_measured_to), fps, rate));
      super.onDestroy();
   }
}
