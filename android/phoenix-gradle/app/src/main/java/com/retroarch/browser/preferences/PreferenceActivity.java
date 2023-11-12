package com.retroarch.browser.preferences;

import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.PreferenceManager;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentPagerAdapter;
import androidx.fragment.app.FragmentTransaction;
import androidx.viewpager.widget.ViewPager;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.ActionBar.Tab;
import androidx.appcompat.app.ActionBar.TabListener;
import androidx.appcompat.app.AppCompatActivity;

import com.retroarch.browser.preferences.fragments.AudioVideoPreferenceFragment;
import com.retroarch.browser.preferences.fragments.GeneralPreferenceFragment;
import com.retroarch.browser.preferences.fragments.PathPreferenceFragment;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarchlite.R;

/**
 * {@link AppCompatActivity} responsible for handling all of the {@link PreferenceListFragment}s.
 * <p>
 * This class can be considered the central activity for the settings, as this class
 * provides the backbone for the {@link ViewPager} that handles all of the fragments being used.
 */
public final class PreferenceActivity extends AppCompatActivity implements TabListener, OnSharedPreferenceChangeListener
{
   // ViewPager for the fragments.
   private ViewPager viewPager;

   public static boolean configDirty;

   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);

      AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);

      // Set the ViewPager.
      setContentView(R.layout.preference_viewpager);
      viewPager = (ViewPager) findViewById(R.id.viewPager);

      // Initialize the ViewPager adapter.
      final PreferencesAdapter adapter = new PreferencesAdapter(getSupportFragmentManager());
      viewPager.setAdapter(adapter);

      // Register the preference change listener.
      final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(this);
      sPrefs.registerOnSharedPreferenceChangeListener(this);

      // Initialize the ActionBar.
      final ActionBar actionBar = getSupportActionBar();
      actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);
      actionBar.addTab(actionBar.newTab().setText(R.string.general_options).setTabListener(this));
      actionBar.addTab(actionBar.newTab().setText(R.string.audio_video_options).setTabListener(this));
      actionBar.addTab(actionBar.newTab().setText(R.string.path_options).setTabListener(this));

      // When swiping between different sections, select the corresponding
      // tab. We can also use ActionBar.Tab#select() to do this if we have
      // a reference to the Tab.
      viewPager.setOnPageChangeListener(new ViewPager.SimpleOnPageChangeListener()
      {
         @Override
         public void onPageSelected(int position)
         {
            actionBar.setSelectedNavigationItem(position);
         }
      } );
   }

   @Override
   public void onTabSelected(Tab tab, FragmentTransaction ft)
   {
      // Switch to the fragment indicated by the tab's position.
      viewPager.setCurrentItem(tab.getPosition());
   }

   @Override
   public void onTabUnselected(Tab tab, FragmentTransaction ft)
   {
      // Do nothing.
   }

   @Override
   public void onTabReselected(Tab tab, FragmentTransaction ft)
   {
      // Do nothing
   }

   @Override
   public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key)
   {
      // TODO: Only do this for prefs that affect retroarch.cfg
      configDirty = true;
   }
   
   @Override
   public void onDestroy()
   {
      if (configDirty)
      {
         UserPreferences.updateConfigFile(this);
         configDirty = false;
      }
      super.onDestroy();
   }

   /**
    * The {@link FragmentPagerAdapter} that will back
    * the view pager of this {@link PreferenceActivity}.
    */
   private final class PreferencesAdapter extends FragmentPagerAdapter
   {
      /**
       * Constructor
       * 
       * @param fm the {@link FragmentManager} for this adapter.
       */
      public PreferencesAdapter(FragmentManager fm)
      {
         super(fm);
      }

      @Override
      public Fragment getItem(int fragmentId)
      {
         switch (fragmentId)
         {
            case 0:
               return new GeneralPreferenceFragment();

            case 1:
               return new AudioVideoPreferenceFragment();

            case 2:
               return new PathPreferenceFragment();

            default: // Should never happen
               return null;
         }
      }

      @Override
      public CharSequence getPageTitle(int position)
      {
         switch (position)
         {
            case 0:
               return getString(R.string.general_options);

            case 1:
               return getString(R.string.audio_video_options);

            case 2:
               return getString(R.string.path_options);

            default: // Should never happen
               return null;
         }
      }

      @Override
      public int getCount()
      {
         return 3;
      }
   }
}
