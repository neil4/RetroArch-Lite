package com.retroarch.browser.preferences.fragments;

import android.os.Bundle;

import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarchlite.R;

/**
 * A {@link PreferenceListFragment} responsible for handling the input preferences.
 */
public final class InputPreferenceFragment extends PreferenceListFragment
{
   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);
      
      // Add input preferences from the XML.
      addPreferencesFromResource(R.xml.input_preferences);
   }
}
