package com.retroarch.browser.coremanager;


import android.os.Bundle;
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
import android.util.Pair;

import com.retroarch.browser.ModuleWrapper;
import com.retroarch.browser.coremanager.fragments.DownloadableCoresFragment;
import com.retroarch.browser.coremanager.fragments.InstalledCoresFragment;
import com.retroarch.browser.coremanager.fragments.InstalledCoresManagerFragment;
import com.retroarch.browser.coremanager.fragments.BackupCoresFragment;
import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarchlite.R;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 * Activity which provides the base for viewing installed cores,
 * as well as the ability to download other cores.
 */
public final class CoreManagerActivity extends AppCompatActivity implements DownloadableCoresFragment.OnCoreDownloadedListener, BackupCoresFragment.OnCoreCopiedListener, InstalledCoresFragment.OnCoreBackupListener, TabListener
{
   // ViewPager for the fragments
   private ViewPager viewPager;
   private InstalledCoresManagerFragment installedCoresFragment = null;
   private BackupCoresFragment backupCoresFragment = null;
   public static DownloadableCoresFragment downloadableCoresFragment = null;

   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);

      AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);

      // Set the ViewPager
      setContentView(R.layout.coremanager_viewpager);
      viewPager = (ViewPager) findViewById(R.id.coreviewer_viewPager);
      
      // Set the ViewPager adapter.
      final ViewPagerAdapter adapter = new ViewPagerAdapter(getSupportFragmentManager());
      viewPager.setAdapter(adapter);

      // Initialize the ActionBar.
      final ActionBar actionBar = getSupportActionBar();
      actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);
      actionBar.addTab(actionBar.newTab().setText(R.string.installed_cores).setTabListener(this));
      actionBar.addTab(actionBar.newTab().setText(R.string.backup_cores).setTabListener(this));
      actionBar.addTab(actionBar.newTab().setText(R.string.downloadable_cores).setTabListener(this));

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
      });

      installedCoresFragment = new InstalledCoresManagerFragment();
      backupCoresFragment = new BackupCoresFragment();
      downloadableCoresFragment = new DownloadableCoresFragment();

      final String dataDir = getApplicationInfo().dataDir;
      
      File dir = new File(dataDir + "/cores/");
      if (!dir.exists())
         dir.mkdir();
      
      dir = new File(dataDir + "/info/");
      if (!dir.exists())
         dir.mkdir();

      MainMenuActivity.checkRuntimePermissions(this);
   }

   @Override
   public void onResume() {
      super.onResume();
      backupCoresFragment.updateList();
   }

   @Override
   public void onDestroy() {
      super.onDestroy();

      if (isFinishing())
         DownloadableCoresFragment.coreList = null;
   }

   @Override
   public void onTabSelected(Tab tab, FragmentTransaction ft)
   {
      // Switch to the fragment indicated by the tab's position.
      viewPager.setCurrentItem(tab.getPosition());
   }

   @Override
   public void onTabReselected(Tab tab, FragmentTransaction ft)
   {
      // Do nothing. Not used.
   }

   @Override
   public void onTabUnselected(Tab tab, FragmentTransaction ft)
   {
      // Do nothing. Not used.
   }

   @Override
   public void onCoreDownloaded()
   {
      updateInstalledCoreList();
   }

   @Override
   public void onCoreCopied()
   {
      updateInstalledCoreList();
   }

   @Override
   public void onCoreBackupCreated()
   {
      backupCoresFragment.updateList();
   }

   private void updateInstalledCoreList()
   {
      InstalledCoresManagerFragment icmf = (InstalledCoresManagerFragment) getSupportFragmentManager().findFragmentByTag("android:switcher:" + R.id.coreviewer_viewPager + ":" + 0);
      if (icmf != null)
      {
         InstalledCoresFragment icf = (InstalledCoresFragment) icmf.getChildFragmentManager().findFragmentByTag("InstalledCoresList");
         if (icf != null)
            icf.updateInstalledCoresList();
      }
   }

   /**
    * @param coreBasename core file basename
    * @return best info filename
    */
   static public String infoBasename(String coreBasename)
   {
      int endIndex = coreBasename.lastIndexOf("_android");
      if (endIndex < 0)
         endIndex = coreBasename.lastIndexOf("_libretro") + 9;
      if (endIndex < 9)
         endIndex = coreBasename.indexOf('.');
      if (endIndex < 0)
         endIndex = coreBasename.length();

      return coreBasename.substring(0, endIndex) + ".info";
   }

   /**
    * @param path relative or absolute core file path
    * @return path without directory or "_libretro_android.so"
    */
   public static String sanitizedLibretroName(String path)
   {
      int startIndex = path.lastIndexOf('/') + 1;
      int endIndex = path.lastIndexOf("_libretro");
      if (endIndex < 0)
         endIndex = path.indexOf('.', startIndex);
      if (endIndex < 0)
         return new String(path);

      return path.substring(startIndex, endIndex);
   }

   /**
    * @param infoFilePath path to core info file
    * @return best core title and mfr+system title
    */
   public static Pair<String,String> getTitlePair(String infoFilePath)
   {
      String name = "";
      String system = "";
      String displayName = "";

      try
      {
         File inputFile = new File(infoFilePath);

         String str;
         BufferedReader br = new BufferedReader(
               new InputStreamReader(new FileInputStream(inputFile)));
         StringBuilder sb = new StringBuilder();

         while ((str = br.readLine()) != null)
            sb.append(str).append("\n");
         br.close();

         String[] lines = sb.toString().split("\n");
         for (String line : lines) {
            if (line.startsWith("corename")) {
               name = line.split("=")[1].trim().replace("\"", "");
               break;
            }
         }
         for (String line : lines) {
            if (line.startsWith("systemname")) {
               system = line.split("=")[1].trim().replace("\"", "");
               break;
            }
         }
         for (String line : lines) {
            if (line.startsWith("display_name")) {
               displayName = line.split("=")[1].trim().replace("\"", "");
               break;
            }
         }

         name = ModuleWrapper.bestCoreTitle(displayName, name);
         system = ModuleWrapper.bestSystemTitle(displayName, system);
      }
      catch (FileNotFoundException fnfe)
      {
         // Can't find the info file. Name it the same thing as the package.
         final int start = infoFilePath.lastIndexOf('/') + 1;
         final int end = infoFilePath.lastIndexOf('.');
         if (end == -1)
            name = system = infoFilePath.substring(start);
         else
            name = system = infoFilePath.substring(start, end);
      }
      catch (IOException ioe)
      {
         name = "Report this: " + ioe.getMessage();
      }

      return new Pair<>(name,system);
   }

   // Adapter for the core manager ViewPager.
   private final class ViewPagerAdapter extends FragmentPagerAdapter
   {
      /**
       * Constructor
       * 
       * @param fm The {@link FragmentManager} for this adapter.
       */
      public ViewPagerAdapter(FragmentManager fm)
      {
         super(fm);
      }

      @Override
      public Fragment getItem(int position)
      {
         switch (position)
         {
            case 0:
               return installedCoresFragment;

            case 1:
               return backupCoresFragment;
               
            case 2:
               return downloadableCoresFragment;

            default: // Should never happen.
               return null;
         }
      }

      @Override
      public int getCount()
      {
         return 3;
      }

      @Override
      public CharSequence getPageTitle(int position)
      {
         switch (position)
         {
            case 0:
               return getString(R.string.installed_cores);
               
            case 1:
               return getString(R.string.backup_cores);

            case 2:
               return getString(R.string.downloadable_cores);

            default: // Should never happen.
               return null;
         }
      }
   }
}
