package com.retroarch.browser.preferences.fragments;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceCategory;

import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarchlite.BuildConfig;
import com.retroarchlite.R;

/**
 * A {@link PreferenceListFragment} that handles the path preferences.
 */
public final class PathPreferenceFragment extends PreferenceListFragment implements OnPreferenceClickListener
{
   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);

      // Add path preferences from the XML.
      addPreferencesFromResource(R.xml.path_preferences);

      // Hide preferences as needed
      PreferenceCategory catBackupCors = (PreferenceCategory) findPreference("cat_backup_cores");
      if (BuildConfig.APPLICATION_ID.contains("64"))
      {
         catBackupCors.removePreference(findPreference("backup_cores32_directory_enable"));
         catBackupCors.removePreference(findPreference("backup_cores32_dir_pref"));
      }
      else
      {
         catBackupCors.removePreference(findPreference("backup_cores64_directory_enable"));
         catBackupCors.removePreference(findPreference("backup_cores64_dir_pref"));
      }

      // Set preference click listeners
      findPreference("rom_dir_pref").setOnPreferenceClickListener(this);
      findPreference("srm_dir_pref").setOnPreferenceClickListener(this);
      findPreference("save_state_dir_pref").setOnPreferenceClickListener(this);
      findPreference("system_dir_pref").setOnPreferenceClickListener(this);
      findPreference("config_dir_pref").setOnPreferenceClickListener(this);

      if (BuildConfig.APPLICATION_ID.contains("64"))
         findPreference("backup_cores64_dir_pref").setOnPreferenceClickListener(this);
      else
         findPreference("backup_cores32_dir_pref").setOnPreferenceClickListener(this);
   }
   
   @Override
   public boolean onPreferenceClick(Preference preference)
   {
      final String prefKey = preference.getKey();

      // Custom ROM directory
      if (prefKey.equals("rom_dir_pref"))
      {
         final DirectoryFragment romDirBrowser = DirectoryFragment.newInstance(R.string.rom_directory_select);
         romDirBrowser.setPathSettingKey("rgui_browser_directory");
         romDirBrowser.setIsDirectoryTarget(true);
         romDirBrowser.show(getFragmentManager(), "romDirBrowser");
      }
      // Custom savefile directory
      else if (prefKey.equals("srm_dir_pref"))
      {
         final DirectoryFragment srmDirBrowser = DirectoryFragment.newInstance(R.string.savefile_directory_select);
         srmDirBrowser.setPathSettingKey("savefile_directory");
         srmDirBrowser.setIsDirectoryTarget(true);
         srmDirBrowser.show(getFragmentManager(), "srmDirBrowser");
      }
      // Custom save state directory
      else if (prefKey.equals("save_state_dir_pref"))
      {
         final DirectoryFragment saveStateDirBrowser = DirectoryFragment.newInstance(R.string.save_state_directory_select);
         saveStateDirBrowser.setPathSettingKey("savestate_directory");
         saveStateDirBrowser.setIsDirectoryTarget(true);
         saveStateDirBrowser.show(getFragmentManager(), "saveStateDirBrowser");
      }
      // Custom system directory
      else if (prefKey.equals("system_dir_pref"))
      {
         final DirectoryFragment systemDirBrowser = DirectoryFragment.newInstance(R.string.system_directory_select);
         systemDirBrowser.setPathSettingKey("system_directory");
         systemDirBrowser.setIsDirectoryTarget(true);
         systemDirBrowser.show(getFragmentManager(), "systemDirBrowser");
      }
      // Custom config directory
      else if (prefKey.equals("config_dir_pref"))
      {
         final DirectoryFragment configDirBrowser = DirectoryFragment.newInstance(R.string.config_directory_select);
         configDirBrowser.setPathSettingKey("rgui_config_directory");
         configDirBrowser.setIsDirectoryTarget(true);
         configDirBrowser.show(getFragmentManager(), "configDirBrowser");
      }
      // Local Installable Cores directory
      else if (prefKey.equals("backup_cores64_dir_pref") || prefKey.equals("backup_cores32_dir_pref"))
      {
         final DirectoryFragment coreDirBrowser = DirectoryFragment.newInstance(R.string.backup_cores_directory_select);
         if (BuildConfig.APPLICATION_ID.contains("64"))
            coreDirBrowser.setPathSettingKey("backup_cores64_directory");
         else
            coreDirBrowser.setPathSettingKey("backup_cores32_directory");
         coreDirBrowser.setIsDirectoryTarget(true);
         coreDirBrowser.show(getFragmentManager(), "coreDirBrowser");
      }

      return true;
   }
}
