package com.retroarch.browser.preferences.fragments;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;

import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarchlite.R;

/**
 * A {@link PreferenceListFragment} responsible for handling the input preferences.
 */
public final class InputPreferenceFragment extends PreferenceListFragment implements OnPreferenceClickListener, DirectoryFragment.OnDirectoryFragmentClosedListener
{
   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);
      
      // Add input preferences from the XML.
      addPreferencesFromResource(R.xml.input_preferences);

      // Set preference listeners
      findPreference("install_overlays_pref").setOnPreferenceClickListener(this);
   }

   @Override
   public boolean onPreferenceClick(Preference preference)
   {
      final String prefKey = preference.getKey();

      if (prefKey.equals("install_overlays_pref"))
      {
         final DirectoryFragment overlayFileBrowser
                 = DirectoryFragment.newInstance("");
         overlayFileBrowser.addAllowedExts("zip");
         overlayFileBrowser.setIsDirectoryTarget(false);
         overlayFileBrowser.setOnDirectoryFragmentClosedListener(this);
         overlayFileBrowser.show(getFragmentManager(), "overlayFileBrowser");
      }

      return true;
   }

   @Override
   public void onDirectoryFragmentClosed(String path)
   {
      DirectoryFragment.ExtractZipWithPrompt(getContext(), path,
            getContext().getApplicationInfo().dataDir + "/overlays", "overlays");
   }
}
