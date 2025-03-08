package com.retroarch.browser.retroactivity;

import android.content.ComponentName;
import android.content.Intent;
import android.view.WindowManager;
import android.view.View;
import android.os.Vibrator;
import android.os.Build;
import android.app.NativeActivity;

import androidx.annotation.Keep;

import com.retroarch.browser.NativeInterface;
import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;

public class RetroActivity extends NativeActivity
{
   View decorView;
   Vibrator vibrator;

   @Override
   public void onLowMemory()
   {
   }

   @Override
   public void onTrimMemory(int level)
   {
   }
   
   @Override
   public void onResume()
   {
      super.onResume();

      MainMenuActivity.retro = getIntent();

      vibrator  = (Vibrator)this.getSystemService(VIBRATOR_SERVICE);
      decorView = getWindow().getDecorView();

      int visibility = View.SYSTEM_UI_FLAG_FULLSCREEN
                       | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                       | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                       | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
      if (Build.VERSION.SDK_INT >= 19)
         visibility |= View.SYSTEM_UI_FLAG_IMMERSIVE
                       | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

      decorView.setSystemUiVisibility(visibility);

      if (Build.VERSION.SDK_INT >= 28)
      {
         getWindow().getAttributes().layoutInDisplayCutoutMode =
               UserPreferences.getPreferences(this).getBoolean("display_over_cutout", true)
                     ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
                     : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
      }
   }

   // Exiting cleanly from NDK seems to be nearly impossible.
   // Have to use exit(0) to avoid weird things happening, even with runOnUiThread() approaches.
   // Use a separate JNI function to explicitly trigger the readback.
   @Keep
   public void onRetroArchExit()
   {
      UserPreferences.readbackConfigFile(this);
      MainMenuActivity.retro = null;

      // Restart launcher menu if option was set
      String appId = getIntent().getStringExtra("EXITTOAPPID");
      if (appId != null)
      {
         Intent mainMenu = new Intent();
         mainMenu.setComponent(new ComponentName(appId,
               "com.retroarch.browser.mainmenu.MainMenuActivity"));
         startActivity(mainMenu);
      }
   }

   @Keep
   public String getVolumePaths(char delim)
   {
      return NativeInterface.getVolumePaths(this, delim);
   }

   @Keep
   public void hapticFeedback(int msec)
   {
      if (msec < 0)
         decorView.performHapticFeedback(android.view.HapticFeedbackConstants.KEYBOARD_TAP);
      else
         vibrator.vibrate(msec);
   }
}
