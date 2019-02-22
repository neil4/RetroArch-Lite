package com.retroarch.browser.coremanager.fragments;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.retroarchlite64.R;
import com.retroarch.browser.ModuleWrapper;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.coremanager.CoreManagerActivity;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.v4.app.ListFragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;
import android.view.MenuInflater;
import android.view.MenuItem;
import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.preferences.util.ConfigFile;
import static com.retroarch.browser.preferences.util.UserPreferences.getPreferences;
import java.io.BufferedInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.net.URLConnection;

/**
 * {@link ListFragment} that displays all of the currently installed cores
 * <p>
 * In terms of layout, this is the fragment that is placed on the
 * left side of the screen within the core manager
 */
public final class InstalledCoresFragment extends ListFragment
{
   // Callback for the interface.
   private OnCoreItemClickedListener callback;

   // Adapter backing this ListFragment.
   private InstalledCoresAdapter adapter;

   /**
    * Interface that a parent fragment must implement
    * in order to display the core info view.
    */
   interface OnCoreItemClickedListener
   {
      /**
       * The action to perform when a core is selected within the list view.
       * 
       * @param position The position of the item in the list.
       * @param core     A reference to the actual {@link ModuleWrapper}
       *                 represented by that list item.
       */
      void onCoreItemClicked(int position, ModuleWrapper core);
   }

   @Override
   public void onActivityCreated(Bundle savedInstanceState)
   {
      super.onActivityCreated(savedInstanceState);

      adapter = new InstalledCoresAdapter(getActivity(), android.R.layout.simple_list_item_2, getInstalledCoresList());
      setListAdapter(adapter);

      // Get the callback. (implemented within InstalledCoresManagerFragment).
      callback = (OnCoreItemClickedListener) getParentFragment();
   }
   
   @Override
   public void onViewCreated(View view, Bundle savedInstanceState)
   {
      super.onViewCreated(view, savedInstanceState);

      registerForContextMenu(getListView());
   }
   
   @Override
   public void onListItemClick(ListView l, View v, int position, long id)
   {
      callback.onCoreItemClicked(position, adapter.getItem(position));

      // Set the item as checked so it highlights in the two-fragment view.
      getListView().setItemChecked(position, true);
   }
   
   @Override
   public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo)
   {
      super.onCreateContextMenu(menu, v, menuInfo);

      menu.setHeaderTitle(R.string.installed_cores_ctx_title);

      MenuInflater inflater = getActivity().getMenuInflater();
      inflater.inflate(R.menu.installed_cores_context_menu, menu);
   }

   @Override
   public boolean onContextItemSelected(MenuItem item)
   {
      final AdapterContextMenuInfo info = (AdapterContextMenuInfo)item.getMenuInfo();

      switch (item.getItemId())
      {
         case R.id.update_core:
            return UpdateCore(info.position);
         case R.id.backup_core:
            return BackupCore(info.position);
         case R.id.reset_core_options:
            return PurgeCoreSettings(info.position);
         case R.id.remove_core:
            return RemoveCore(info.position);

         default:
            return super.onContextItemSelected(item);
      }
   }

   /**
    * Refreshes the list of installed cores.
    */
   public void updateInstalledCoresList()
   {
      if (adapter == null)
         return;
      
      adapter.clear();
      for (int i = 0; i < getInstalledCoresList().size(); i++)
      {
         adapter.add(getInstalledCoresList().get(i));
      }
      adapter.notifyDataSetChanged();
   }

   private List<ModuleWrapper> getInstalledCoresList()
   {
      // The list of items that will be added to the adapter backing this ListFragment.
      final List<ModuleWrapper> items = new ArrayList<ModuleWrapper>();
      
      // Populate the list
      final File[] libs = new File(getActivity().getApplicationInfo().dataDir, "/cores").listFiles();
      for (File lib : libs)
         items.add(new ModuleWrapper(getActivity(), lib));

      // Sort the list alphabetically
      Collections.sort(items);
      return items;
   }

   // This will be the handler for long clicks on individual list items in this ListFragment.
   public boolean RemoveCore(int position)
   {
      // Begin building the AlertDialog
      final ModuleWrapper item = adapter.getItem(position);
      final AlertDialog.Builder alert = new AlertDialog.Builder(getActivity());
      alert.setTitle(R.string.confirm_title);
      alert.setMessage(String.format(getString(R.string.uninstall_core_message), item.getText()));
      alert.setNegativeButton(R.string.no, null);
      alert.setPositiveButton(R.string.yes, new OnClickListener()
      {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Attempt to uninstall the core item.
            if (item.getUnderlyingFile().delete())
            {
               // Remove ROM search directory preference also
               String key_prefix = sanitizedLibretroName(item.getUnderlyingFile().getName()) + "_";
               final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
               prefs.edit().remove(key_prefix + "directory").commit();
               
               Toast.makeText(getActivity(), String.format(getString(R.string.uninstall_success), item.getText()), Toast.LENGTH_LONG).show();
               adapter.remove(item);
               adapter.notifyDataSetChanged();
            }
            else // Failed to uninstall.
            {
               Toast.makeText(getActivity(), String.format(getString(R.string.uninstall_failure), item.getText()), Toast.LENGTH_LONG).show();
            }
         }
      });
      alert.show();

      return true;
   }
   
   /**
    * Removes all retroarch.cfg entries specific to this core.
    * Also deletes core's config folder, which holds core-provided options, ROM specific configs, and input remapping files.
    * @param position list position of current core 
    * @return true
    */
   public boolean PurgeCoreSettings(int position)
   {
      // Begin building the AlertDialog
      final ModuleWrapper item = adapter.getItem(position);
      final AlertDialog.Builder alert = new AlertDialog.Builder(getActivity());
      alert.setTitle(R.string.confirm_title);
      alert.setMessage(String.format(getString(R.string.reset_core_options_message), item.getText()));
      alert.setNegativeButton(R.string.no, null);
      alert.setPositiveButton(R.string.yes, new OnClickListener()
      {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            final String default_base = Environment.getExternalStorageDirectory().getAbsolutePath() + "/RetroArchLite";
            final String default_config = default_base + "/config";
            final SharedPreferences prefs = getPreferences(getContext());
            String cfg_dir = prefs.getBoolean("config_directory_enable", false) ?
                       prefs.getString("rgui_config_directory", default_config) : default_config;
                        
            String libretro_name = sanitizedLibretroName(item.getUnderlyingFile().getName());
               
            String settingsFilePath = getContext().getApplicationInfo().dataDir
                                      + "/retroarch.cfg";
            String optionsDirPath = cfg_dir + '/' + libretro_name;
            
            // Purge both 32- and 64-bit settings, for simplicity
            for (int i = 0; i < 2; i++)
            {
               if (i == 1)
                  settingsFilePath = settingsFilePath.replaceFirst("retroarchlite64", "retroarchlite");

               ConfigFile settingsCfg = new ConfigFile(settingsFilePath);
               if ( new File(settingsFilePath).exists() )
               {
                  settingsCfg.removeKeysWithPrefix(libretro_name + "_");
                  try
                  { settingsCfg.write(settingsFilePath); }
                  catch (IOException e)
                  { Log.e("Core Manager", "Failed to update config file: " + settingsFilePath); }
               }
            }
            
            // Remove core's config folder
            DirectoryFragment.DeleteDirTree(new File(optionsDirPath));
            
            Toast.makeText(getActivity(), String.format(getString(R.string.reset_core_settings_success), item.getText()), Toast.LENGTH_LONG).show();
         }
      });
      alert.show();

      return true;
   }
   
   /**
    * @param path (not modified) relative or absolute core file path
    * @return path without directory or "_android.so" or "_libretro_android.so"
    */
   public static String sanitizedLibretroName(String path)
   {
      String sanitized_name = path.substring(
            path.lastIndexOf("/") + 1,
            path.lastIndexOf("."));
      sanitized_name = sanitized_name.replace("libretro_", "");
      sanitized_name = sanitized_name.replace("_android", "");

      return sanitized_name;
   }

   /**
    * The {@link ArrayAdapter} that backs this InstalledCoresFragment.
    */
   private final class InstalledCoresAdapter extends ArrayAdapter<ModuleWrapper>
   {
      private final Context context;
      private final int resourceId;
      private final List<ModuleWrapper> items;

      /**
       * Constructor
       * 
       * @param context    The current {@link Context}.
       * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
       * @param items      The list of items to represent in this adapter.
       */
      public InstalledCoresAdapter(Context context, int resourceId, List<ModuleWrapper> items)
      {
         super(context, resourceId, items);

         this.context = context;
         this.resourceId = resourceId;
         this.items = items;
      }

      @Override
      public ModuleWrapper getItem(int i)
      {
         return items.get(i);
      }

      @Override
      public View getView(int position, View convertView, ViewGroup parent)
      {
         if (convertView == null)
         {
            LayoutInflater vi = LayoutInflater.from(context);
            convertView = vi.inflate(resourceId, parent, false);
         }

         final ModuleWrapper item = items.get(position);
         if (item != null)
         {
            TextView title    = (TextView) convertView.findViewById(android.R.id.text1);
            TextView subtitle = (TextView) convertView.findViewById(android.R.id.text2);

            if (title != null)
            {
               title.setText(item.getText());
            }

            if (subtitle != null)
            {
               subtitle.setText(item.getSubText());
            }
         }

         return convertView;
      }
   }
   
   /**
    * Update core to the latest libretro.com nightly build
    * @param position list position of current core
    * @return true
    */
   public boolean UpdateCore(int position)
   {
      final String coreFileName = adapter.getItem(position).getUnderlyingFile().getName();
      final String coreURL = (Build.SUPPORTED_ABIS[0].startsWith("arm") ?
                              DownloadableCoresFragment.BUILDBOT_CORE_URL_ARM
                              : DownloadableCoresFragment.BUILDBOT_CORE_URL_INTEL)
                             + coreFileName.concat(".zip");

      final DownloadableCore core = new DownloadableCore(adapter.getItem(position).getText(), "", coreURL);

      // Prompt the user for confirmation on updating the core.
      AlertDialog.Builder notification = new AlertDialog.Builder(getActivity());
      notification.setMessage(String.format(getString(R.string.update_core_confirm_msg), core.getCoreName()));
      notification.setTitle(R.string.confirm_title);
      notification.setNegativeButton(R.string.no, null);
      notification.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Begin downloading the core.
            CoreManagerActivity.downloadableCoresFragment.DownloadCore(getContext(), core);
         }
      });
      notification.show();
      
      return true;
   }
   
   /**
    * Copy core's .so and .info files to the Backup Core directory
    * @param position list position of current core
    * @return true
    */
   public boolean BackupCore(int position)
   {
      // Begin building the AlertDialog
      final ModuleWrapper item = adapter.getItem(position);
      final AlertDialog.Builder alert = new AlertDialog.Builder(getActivity());
      
      SharedPreferences settings = UserPreferences.getPreferences(getActivity());
      final File destFile
         = new File( settings.getString("backup_cores_directory", LocalCoresFragment.defaultLocalCoresDir),
                     File.separator+item.getUnderlyingFile().getName() );
      boolean exists = destFile.exists();
      
      if (!destFile.getParentFile().exists())
         destFile.getParentFile().mkdirs();

      alert.setTitle(R.string.confirm_title);
      alert.setMessage(String.format(getString(R.string.backup_core_message)
                       + (exists ? "\nBackup exists and will be overwritten.": ""), item.getText()));
      alert.setNegativeButton(R.string.no, null);
      alert.setPositiveButton(R.string.yes, new OnClickListener()
      {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Begin backup
            Toast.makeText(getActivity(), "Writing " + destFile.getAbsolutePath(), Toast.LENGTH_SHORT).show();

            new BackupCoreOperation( getActivity(),
                                     item.getUnderlyingFile().getName() )
                .execute( item.getUnderlyingFile().getAbsolutePath(),
                          item.getUnderlyingFile().getName() );
         }
      });
      alert.show();

      return true;
   }
   
   /**
    * Executed when the user confirms a core backup.
    */
   private final class BackupCoreOperation extends AsyncTask<String, Integer, Void>
   {
      private final ProgressDialog dlg;
      private final Context ctx;
      private final String coreName;

      /**
       * Constructor
       *
       * @param ctx      The current {@link Context}.
       * @param coreName The name of the core being downloaded.
       */
      public BackupCoreOperation(Context ctx, String coreName)
      {
         this.dlg = new ProgressDialog(ctx);
         this.ctx = ctx;
         this.coreName = coreName;
      }

      @Override
      protected void onPreExecute()
      {
         super.onPreExecute();
         
         dlg.setMessage(String.format(ctx.getString(R.string.backing_up_msg), coreName));
         dlg.setCancelable(false);
         dlg.setCanceledOnTouchOutside(false);
         dlg.setIndeterminate(false);
         dlg.setMax(100);
         dlg.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
         dlg.show();
      }
      
      @Override
      protected Void doInBackground(String... params)
      {
         InputStream input = null;
         OutputStream output = null;
         URLConnection connection;
         SharedPreferences settings = UserPreferences.getPreferences(getActivity());
         
         try
         {
            URL url = new URL("file:"+params[0]);  // input URL

            connection = (URLConnection) url.openConnection();
            connection.connect();

            // Set up the streams
            final int fileLen = connection.getContentLength();
            final File outFile = new File( settings.getString("backup_cores_directory",
                                                              LocalCoresFragment.defaultLocalCoresDir),
                                           File.separator+params[1] );

            input = new BufferedInputStream(connection.getInputStream(), 8192);
            output = new FileOutputStream(outFile);            

            // Copy core to local cores directory.
            //
            long totalCopied = 0;
            byte[] buffer = new byte[4096];
            int countBytes;
            while ((countBytes = input.read(buffer)) != -1)
            {
               totalCopied += countBytes;
               if (fileLen > 0)
                  publishProgress((int) (totalCopied * 100 / fileLen));

               output.write(buffer, 0, countBytes);
            }
            input.close();
            output.close();
            
            { // Now copy the info file
               String infoPath = params[0].replace("/cores/","/info/")
                                          .replace("_android.so", ".info");
               if (new File(infoPath).exists())
               {
                  url = new URL("file:" + infoPath);  // input URL
                  connection = (URLConnection) url.openConnection();
                  final File outInfoFile
                  = new File( settings.getString("backup_cores_directory", LocalCoresFragment.defaultLocalCoresDir),
                              File.separator+params[1].replace("_android.so",".info") );
                  connection.connect();
                  input = new BufferedInputStream(connection.getInputStream(), 8192);
                  output = new FileOutputStream(outInfoFile); 
                  while ((countBytes = input.read(buffer)) != -1)
                     output.write(buffer, 0, countBytes);
                  input.close();
                  output.close();
               }
            }

         }
         catch (IOException ignored) {}
         finally
         {
            try
            {
               if (output != null)
                  output.close();

               if (input != null)
                  input.close();
            }
            catch (IOException ignored) {}
         }

         return null;
      }

      @Override
      protected void onProgressUpdate(Integer... progress)
      {
         super.onProgressUpdate(progress);
         dlg.setProgress(progress[0]);
      }

      @Override
      protected void onPostExecute(Void result)
      {
         super.onPostExecute(result);
         dlg.dismiss();
      }
   }
}

