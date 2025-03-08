package com.retroarch.browser.dirfragment;

import android.content.SharedPreferences;
import android.graphics.Point;
import android.os.Bundle;
import android.os.Environment;
import android.os.Parcel;
import android.os.Parcelable;
import androidx.fragment.app.DialogFragment;
import android.view.Display;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import com.retroarch.browser.DarkToast;
import com.retroarch.browser.FileWrapper;
import com.retroarch.browser.IconAdapter;
import com.retroarch.browser.NativeInterface;
import com.retroarch.browser.preferences.PreferenceActivity;
import com.retroarch.browser.preferences.util.ConfigFile;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarchlite.R;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;


/**
 * {@link DialogFragment} subclass that provides a file-browser
 * like UI for browsing for specific files.
 * <p>
 * This file browser also allows for custom filtering
 * depending on the type of class that inherits it.
 * <p>
 * This file browser also uses an implementation of a 
 * backstack for remembering previously browsed folders
 * within this DirectoryFragment.
 * <p>
 * To instantiate a new instance of this class
 * you must use the {@code newInstance} method.
 */
public class DirectoryFragment extends DialogFragment
{
   protected IconAdapter<FileWrapper> adapter;
   protected File listedDirectory;

   protected ArrayList<BackStackItem> backStack;
   protected String startDirectory;
   protected String pathSettingKey;
   protected boolean isDirectoryTarget;
   private ArrayList<String> allowedExt;
   protected static OnDirectoryFragmentClosedListener onClosedListener;

   private boolean showMameTitles;
   private String fileListPath = null;

   public static ConfigFile mameListFile = null;

   public static final class BackStackItem implements Parcelable
   {
      protected final String path;
      protected final boolean parentIsBack;

      public BackStackItem(String path, boolean parentIsBack)
      {
         this.path = path;
         this.parentIsBack = parentIsBack;
      }

      private BackStackItem(Parcel in)
      {
         this.path = in.readString();
         this.parentIsBack = in.readInt() != 0;
      }

      public int describeContents()
      {
         return 0;
      }

      public void writeToParcel(Parcel out, int flags)
      {
         out.writeString(path);
         out.writeInt(parentIsBack ? 1 : 0);
      }

      public static final Parcelable.Creator<BackStackItem> CREATOR = new Parcelable.Creator<BackStackItem>()
      {
         public BackStackItem createFromParcel(Parcel in)
         {
            return new BackStackItem(in);
         }

         public BackStackItem[] newArray(int size)
         {
            return new BackStackItem[size];
         }
      };
   }

   /**
    * Listener interface for executing content or performing
    * other things upon the DirectoryFragment instance closing.
    */
   public interface OnDirectoryFragmentClosedListener
   {
      /**
       * Performs some arbitrary action after the 
       * {@link DirectoryFragment} closes.
       * 
       * @param path The path to the file chosen within the {@link DirectoryFragment}
       */
      void onDirectoryFragmentClosed(String path);
   }

   /**
    * Sets the starting directory for this DirectoryFragment
    * when it is shown to the user.
    * 
    * @param path the initial directory to show to the user
    *             when this DirectoryFragment is shown.
    */
   public void setStartDirectory(String path)
   {
      startDirectory = path;
   }

   /**
    * Sets the key to save the selected item in the DialogFragment
    * into the application SharedPreferences at.
    * 
    * @param key the key to save the selected item's path to in
    *            the application's SharedPreferences.
    */
   public void setPathSettingKey(String key)
   {
      pathSettingKey = key;
   }

   /**
    * Sets whether or not we are browsing for a specific
    * directory or not. If enabled, it will allow the user
    * to select a specific directory, rather than a file.
    * 
    * @param enable Whether or not to enable this.
    */
   public void setIsDirectoryTarget(boolean enable)
   {
      isDirectoryTarget = enable;
   }

   /**
    * Sets the listener for an action to perform upon the
    * closing of this DirectoryFragment.
    * 
    * @param onClosedListener the OnDirectoryFragmentClosedListener to set.
    */
   public void setOnDirectoryFragmentClosedListener(OnDirectoryFragmentClosedListener onClosedListener)
   {
      this.onClosedListener = onClosedListener;
   }

   /**
    * Retrieves a new instance of a DirectoryFragment
    * with a title specified by the given resource ID.
    * 
    * @param titleResId String resource ID for the title
    *                   of this DirectoryFragment.
    * 
    * @return A new instance of a DirectoryFragment.
    */
   public static DirectoryFragment newInstance(int titleResId)
   {
      final DirectoryFragment dFrag = new DirectoryFragment();
      final Bundle bundle = new Bundle();
      bundle.putInt("titleResId", titleResId);
      dFrag.setArguments(bundle);
      onClosedListener = null;

      return dFrag;
   }

   /**
    * Retrieves a new instance of a DirectoryFragment
    * with a title specified by the given string.
    * 
    * @param title String for the title of this DirectoryFragment.
    * 
    * @return A new instance of a DirectoryFragment.
    */
   public static DirectoryFragment newInstance(String title)
   {
      final DirectoryFragment dFrag = new DirectoryFragment();
      final Bundle bundle = new Bundle();
      bundle.putInt("titleResId", -88);
      bundle.putString("title", title);
      dFrag.setArguments(bundle);
      onClosedListener = null;

      return dFrag;
   }

   public static DirectoryFragment newInstance(String title, List<String> exts)
   {
      final DirectoryFragment dFrag = new DirectoryFragment();
      final Bundle bundle = new Bundle();
      bundle.putInt("titleResId", -88);
      bundle.putString("title", title);
      
      String[] extArray = new String[exts.size()];
      int i = 0;
      for (String ext : exts)
      {
         if (ext == null)
            return null;
         extArray[i++] = ext.toLowerCase();
      }
      bundle.putStringArray("exts", extArray);

      if (extArray.length > 0)
         dFrag.addAllowedExts(extArray);
      dFrag.setArguments(bundle);
      onClosedListener = null;

      return dFrag;
   }

   public void setShowMameTitles(boolean showMameTitles)
   {
      this.showMameTitles = showMameTitles;
   }

   public void setFileListPath(String path)
   {
      this.fileListPath = path;
   }

   @Override
   public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
   {
      ListView rootView = (ListView) inflater.inflate(R.layout.line_list, container, false);
      rootView.setOnItemClickListener(onItemClickListener);

      if (savedInstanceState != null)
      {
         backStack = savedInstanceState.getParcelableArrayList("BACKSTACK");
         showMameTitles = savedInstanceState.getBoolean("MAMETITLES");
         isDirectoryTarget = savedInstanceState.getBoolean("DIRTARGET");
         pathSettingKey = savedInstanceState.getString("PATHSETTINGKEY");
         allowedExt = savedInstanceState.getStringArrayList("EXTS");
         fileListPath = savedInstanceState.getString("PATHLIST");
      }

      // Set the dialog title.
      if (getArguments().getInt("titleResId") == -88)
         getDialog().setTitle(getArguments().getString("title"));
      else
         getDialog().setTitle(getArguments().getInt("titleResId"));
      
      if (pathSettingKey != null && !pathSettingKey.isEmpty())
      {
         DarkToast.makeText(getActivity(), "Current Directory:\n"
                     + UserPreferences.getPreferences(getActivity())
                     .getString(pathSettingKey, "<Default>"));
      }

      // Setup the list
      adapter = new IconAdapter<FileWrapper>(getActivity(), R.layout.line_list_item);
      rootView.setAdapter(adapter);

      if (backStack == null || backStack.isEmpty())
      {
         backStack = new ArrayList<BackStackItem>();
         String startPath = (startDirectory == null || startDirectory.isEmpty()) ?
            Environment.getExternalStorageDirectory().getPath() : startDirectory;
         
         backStack.add(new BackStackItem(startPath, false));
      }

      if (showMameTitles && mameListFile == null)
      {  // Read mamelist.txt
         String mameListPath = getContext().getApplicationInfo().dataDir
               + "/info/mamelist.txt";
         mameListFile = new ConfigFile(mameListPath);
      }

      if (fileListPath != null)
         wrapFileList();
      else
         wrapFiles();

      return rootView;
   }
   
   @Override
   public void onResume()
   {
      super.onResume();
      final Display display = getActivity().getWindowManager().getDefaultDisplay();
      Point size = new Point();
      display.getSize(size);

      WindowManager.LayoutParams params = getDialog().getWindow().getAttributes();
      if (size.x > size.y)
         params.width = (65 * size.x) / 100;
      else
         params.width = (85 * size.x) / 100;

      getDialog().getWindow().setAttributes(params);
   }

   private final OnItemClickListener onItemClickListener = new OnItemClickListener()
   {
      @Override
      public void onItemClick(AdapterView<?> parent, View view, int position, long id)
      {
         final FileWrapper item = adapter.getItem(position);

         if (item.isParentItem() && backStack.get(backStack.size() - 1).parentIsBack)
         {
            backStack.remove(backStack.size() - 1);
            wrapFiles();
            return;
         }
         else if (item.isDirSelectItem())
         {
            finishWithPath(listedDirectory.getAbsolutePath());
            DarkToast.makeText(getActivity(), "Selected Directory:\n"
                  + listedDirectory.getAbsolutePath());
            return;
         }

         final File selected = item.isParentItem() ? listedDirectory.getParentFile() : item.getFile();

         if (selected.isDirectory())
         {
            backStack.add(new BackStackItem(selected.getAbsolutePath(), !item.isParentItem()));
            wrapFiles();
         }
         else
         {
            String filePath = selected.getAbsolutePath();
            finishWithPath(filePath);
         }
      }
   };

   @Override
   public void onSaveInstanceState(Bundle outState)
   {
      super.onSaveInstanceState(outState);

      outState.putParcelableArrayList("BACKSTACK", backStack);
      outState.putBoolean("MAMETITLES", showMameTitles);
      outState.putBoolean("DIRTARGET", isDirectoryTarget);
      outState.putStringArrayList("EXTS", allowedExt);
      outState.putString("PATHSETTINGKEY", pathSettingKey);
      outState.putString("PATHLIST", fileListPath);
   }

   private void finishWithPath(String path)
   {
      if (onClosedListener != null)
      {
         onClosedListener.onDirectoryFragmentClosed(path);
      }
      else if (pathSettingKey != null && !pathSettingKey.isEmpty())
      {
         SharedPreferences settings = UserPreferences.getPreferences(getActivity());
         SharedPreferences.Editor editor = settings.edit();
         editor.putString(pathSettingKey, path).apply();
         PreferenceActivity.configDirty = true;
      }

      dismiss();
   }

   // TODO: Hook this up to a callable interface (if backstack is desirable).
   public boolean onKeyDown(int keyCode, KeyEvent event)
   {
      if (keyCode == KeyEvent.KEYCODE_BACK)
      {
         if (backStack.size() > 1)
         {
            backStack.remove(backStack.size() - 1);
            wrapFiles();
         }

         return true;
      }

      return false;
   }

   private boolean filterPath(String path)
   {
      path = path.toLowerCase();

      if (allowedExt != null)
      {
         for (String ext : allowedExt)
         {
            if (path.endsWith(ext))
               return true;
         }

         return false;
      }

      return true;
   }

   /**
    * Allows specifying allowed file extensions.
    * <p>
    * Any files that contain these file extension will be shown
    * within the DirectoryFragment file browser. Those that don't
    * contain any of these extensions will not be shown.
    * 
    * @param exts The file extension(s) to allow being shown in this DirectoryFragment.
    */
   public void addAllowedExts(String... exts)
   {
      if (allowedExt == null)
         allowedExt = new ArrayList<String>();

      allowedExt.addAll(Arrays.asList(exts));
   }

   protected void wrapFiles()
   {
      listedDirectory = new File(backStack.get(backStack.size() - 1).path);
      String volList;

      if (!listedDirectory.isDirectory())
      {
         throw new IllegalArgumentException("Directory is not valid.");
      }

      adapter.clear();
      
      if (isDirectoryTarget)
         adapter.add(new FileWrapper(null, FileWrapper.DIRSELECT, true));

      if (listedDirectory.getParentFile() != null)
         adapter.add(new FileWrapper(null, FileWrapper.PARENT, true));

      // Copy new items. If none, list storage volumes.
      final File[] files = listedDirectory.listFiles();
      if (files != null)
      {
         for (File file : files)
         {
            String path = file.getName();

            boolean allowFile = file.isDirectory() || (filterPath(path) && !isDirectoryTarget);
            if (allowFile)
            {
               boolean showMameTitle = showMameTitles
                     && (path.endsWith(".zip") || path.endsWith((".7z")));
               adapter.add(new FileWrapper(file, FileWrapper.FILE, true,
                     showMameTitle ? mameListFile : null));
            }
         }
      }
      else if (!(volList = NativeInterface.getVolumePaths(getContext(), '|')).isEmpty())
      {
         adapter.clear();
         String[] tokens = volList.split("\\|");
         for (String token : tokens)
            adapter.add(new FileWrapper(new File(token), FileWrapper.FILE, true));
      }

      // Sort items
      adapter.sort(new Comparator<FileWrapper>()
      {
         @Override
         public int compare(FileWrapper left, FileWrapper right)
         {
            return left.compareTo(right);
         }
      });
      
      // Update
      adapter.notifyDataSetChanged();
   }

   protected void wrapFileList()
   {
      adapter.clear();

      try
      {
         String item;
         BufferedReader br = new BufferedReader(
               new InputStreamReader(new FileInputStream(fileListPath)));

         while ((item = br.readLine()) != null)
         {
            boolean showMameTitle = showMameTitles
                  && (item.endsWith(".zip") || item.endsWith((".7z")));
            adapter.add(new FileWrapper(new File(item), FileWrapper.FILE, true,
                  showMameTitle ? mameListFile : null));
         }
         br.close();
      }
      catch (Exception e)
      {
         // TODO
      }

      // Update
      adapter.notifyDataSetChanged();
   }
}
