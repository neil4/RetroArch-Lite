package com.retroarch.browser.retroactivity;

import android.view.View;
import android.os.Build;
import android.app.NativeActivity;

import androidx.annotation.Keep;

import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;

public class RetroActivity extends NativeActivity
{
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

      View thisView = getWindow().getDecorView();
      int visibility = View.SYSTEM_UI_FLAG_FULLSCREEN
                       | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                       | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                       | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
      if (Build.VERSION.SDK_INT >= 19)
         visibility |= View.SYSTEM_UI_FLAG_IMMERSIVE
                       | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

      thisView.setSystemUiVisibility(visibility);
   }

   // Exiting cleanly from NDK seems to be nearly impossible.
   // Have to use exit(0) to avoid weird things happening, even with runOnUiThread() approaches.
   // Use a separate JNI function to explicitly trigger the readback.
   @Keep
   public void onRetroArchExit()
   {
      UserPreferences.readbackConfigFile(this);
   }

   @Keep
   public int getRotation()
   {
      return getWindowManager().getDefaultDisplay().getRotation();
   }
}
