package com.retroarch.browser.preferences.fragments;

import android.content.ComponentName;
import android.content.Intent;
import com.retroarchlite.R;
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
      if (have64bitCoreManager())
         addPreferencesFromResource(R.xml.general_preferences_32_64);
      else
         addPreferencesFromResource(R.xml.general_preferences);
   }
   
   protected boolean have64bitCoreManager()
   {
      Intent intent = new Intent();
      intent.setComponent(new ComponentName("com.retroarchlite64",
                                            "com.retroarch.browser.coremanager.CoreManagerActivity"));
      return !getContext().getPackageManager()
                          .queryIntentActivities(intent, 0)
                          .isEmpty();
   }
}
