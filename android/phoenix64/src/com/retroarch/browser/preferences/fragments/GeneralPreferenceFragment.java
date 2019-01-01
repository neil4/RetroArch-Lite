package com.retroarch.browser.preferences.fragments;

import android.content.ComponentName;
import android.content.Intent;
import com.retroarchlite64.R;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.os.Bundle;

/**
 * A {@link PreferenceListFragment} that handles the general settings.
 */
public final class GeneralPreferenceFragment extends PreferenceListFragment
{
   
   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);

      // Add general preferences from the XML.
      if (have32bitCoreManager())
         addPreferencesFromResource(R.xml.general_preferences_32_64);
      else
         addPreferencesFromResource(R.xml.general_preferences);
   }
   
   protected boolean have32bitCoreManager()
   {
      Intent intent = new Intent();
      intent.setComponent(new ComponentName("com.retroarchlite",
                                            "com.retroarch.browser.coremanager.CoreManagerActivity"));
      return !getContext().getPackageManager()
                          .queryIntentActivities(intent, 0)
                          .isEmpty();
   }
}
