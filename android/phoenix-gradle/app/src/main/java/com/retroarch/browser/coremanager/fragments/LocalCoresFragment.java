package com.retroarch.browser.coremanager.fragments;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.app.ListFragment;
import android.util.Log;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import com.retroarch.browser.preferences.util.UserPreferences;

import com.retroarchlite.R;
import java.net.URLConnection;

/**
 * {@link ListFragment} that is responsible for showing
 * cores that are able to be downloaded or are not installed.
 */
public final class LocalCoresFragment extends ListFragment
{
   /**
    * Dictates what actions will occur when a core download completes.
    * <p>
    * Acts like a callback so that communication between fragments is possible.
    */
   public interface OnCoreDownloadedListener
   {
      /** The action that will occur when a core is successfully downloaded. */
      void onCoreDownloaded();
   }

   private String localCoresDir;
   public static String defaultLocalCoresDir = Environment.getExternalStorageDirectory().getPath() + "/RetroArchLite/cores32";
   
   private ListView coreList = null;
   private OnCoreDownloadedListener coreDownloadedListener = null;
   
   protected String systemName;
   protected DownloadableCoresAdapter adapter = null;
   private static SharedPreferences sharedSettings = null;
   
   private static boolean zipHasInfoFile = false;
   
   @Override
   public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
   {
      super.onCreateView(inflater, container, savedInstanceState);
      final String sharedId = getString(R.string.app_id);
      if (sharedId.contains("64"))
         defaultLocalCoresDir = defaultLocalCoresDir.replace("cores32", "cores64");
      sharedSettings = UserPreferences.getPreferences(getActivity());
      localCoresDir = sharedSettings.getString("backup_cores_directory",
                                               defaultLocalCoresDir);
         
      coreList = (ListView) inflater.inflate(R.layout.coremanager_listview, container, false);
      registerForContextMenu(coreList);

      adapter = new DownloadableCoresAdapter(getActivity(), android.R.layout.simple_list_item_2);
      coreList.setAdapter(adapter);

      coreDownloadedListener = (OnCoreDownloadedListener) getActivity();

      PopulateCoresList();
      
      return coreList;
   }

   public void updateList()
   {
      if (adapter != null && sharedSettings != null)
      {
         localCoresDir = sharedSettings.getString("backup_cores_directory", defaultLocalCoresDir);
         adapter.clear();
         PopulateCoresList();
         adapter.notifyDataSetChanged();
      }
   }
   
   @Override
   public void onDestroy()
   {
      super.onDestroy();
   }
   
   @Override
   public void onListItemClick(final ListView lv, final View v, final int position, final long id)
   {
      super.onListItemClick(lv, v, position, id);
      final DownloadableCore core = (DownloadableCore) lv.getItemAtPosition(position);

      // Prompt the user for confirmation on downloading the core.
      AlertDialog.Builder notification = new AlertDialog.Builder(getActivity());
      notification.setMessage(String.format(getString(R.string.backup_cores_confirm_install_msg),
                              (core.getCoreName().isEmpty() ? "this core" : core.getCoreName()) ));
      notification.setTitle(R.string.confirm_title);
      notification.setNegativeButton(R.string.no, null);
      notification.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Begin downloading the core.
            new DownloadCoreOperation(getActivity(), core.getCoreName()).execute(core.getCoreURL(), core.getShortURLName());
         }
      });
      notification.show();
   }
   
   @Override
   public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo)
   {
      super.onCreateContextMenu(menu, v, menuInfo);

      menu.setHeaderTitle(R.string.backup_cores_ctx_title);

      MenuInflater inflater = getActivity().getMenuInflater();
      inflater.inflate(R.menu.backup_cores_context_menu, menu);
   }

   @Override
   public boolean onContextItemSelected(MenuItem item)
   {
      final AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo)item.getMenuInfo();

      switch (item.getItemId())
      {
         case R.id.remove_localcore:
            return RemoveCore(info.position);

         default:
            return super.onContextItemSelected(item);
      }
   }
   
   void PopulateCoresList()
   {
      try
      {
         final File[] files = new File(localCoresDir).listFiles();
         final ArrayList<DownloadableCore> cores = new ArrayList<DownloadableCore>();

         for (int i = 0; i < files.length; i++)
         {
            String urlPath = files[i].toURI().toURL().toString();
            String coreName = files[i].getName();

            boolean is_zip = coreName.endsWith(".zip");
            if (!is_zip && !coreName.endsWith(".so"))
               continue;

            // Allow any name ending in .so or .zip
            String searchStr = (coreName.contains("_android.") ? "_android" : "")
                               + (is_zip ? (coreName.contains(".so.") ? ".so.zip" : ".zip" ) : ".so" );
            String infoPath = localCoresDir + File.separator + coreName.replace(searchStr, ".info");

            cores.add(new DownloadableCore(getCoreName(infoPath), systemName, urlPath));
         }

         Collections.sort(cores);
         adapter.clear();
         adapter.addAll(cores);
      }
      catch (Exception e)
      {
         Log.e("PopulateCoresList", e.toString());
      }
   }
   
   // gets name from .info filePath
   private String getCoreName(String filePath)
   {
      String name = "";
      systemName = "";

      try
      {
         File inputFile = new File(filePath);
         File installedFile = new File( adapter.getContext().getApplicationInfo().dataDir + "/info/",
                                        filePath.substring(filePath.lastIndexOf(File.separator) + 1));
         URL url;
         if (inputFile.exists())
            url = inputFile.toURI().toURL();
         else if (installedFile.exists())
            url = installedFile.toURI().toURL();
         else
            return name;
         
         String str;
         BufferedReader br = new BufferedReader(new InputStreamReader(url.openStream()));
         StringBuilder sb = new StringBuilder();
         while ((str = br.readLine()) != null)
            sb.append(str).append("\n");
         br.close();

         // Now read the core name
         String[] lines = sb.toString().split("\n");
         for (String line : lines) {
            if (line.contains("corename")) {
               name = line.split("=")[1].trim().replace("\"", "");
               break;
            }
         }
         for (String line : lines) {
            if (line.contains("systemname")) {
               systemName = line.split("=")[1].trim().replace("\"", "");
               break;
            }
         }
      }
      catch (FileNotFoundException fnfe)
      {
         // Can't find the info file. Name it the same thing as the package.
         final int start = filePath.lastIndexOf('/') + 1;
         final int end = filePath.lastIndexOf('.');
         if (end == -1)
            name = filePath.substring(start);
         else
            name = filePath.substring(start, end);
      }
      catch (IOException ioe)
      {
         name = "Report this: " + ioe.getMessage();
      }

      return name;
   }


   // Executed when the user confirms a core download.
   private final class DownloadCoreOperation extends AsyncTask<String, Integer, Void>
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
      public DownloadCoreOperation(Context ctx, String coreName)
      {
         this.dlg = new ProgressDialog(ctx);
         this.ctx = ctx;
         this.coreName = coreName;
      }

      @Override
      protected void onPreExecute()
      {
         super.onPreExecute();

         dlg.setMessage(String.format(ctx.getString(R.string.installing_msg), coreName));
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
         zipHasInfoFile = false;
         
         try
         {
            URL url = new URL(params[0]);
            
            connection = (URLConnection) url.openConnection();
            connection.connect();
            
            // Set up the streams
            final File zipFile = new File(ctx.getApplicationInfo().dataDir + "/cores/", params[1]);
            final int fileLen = connection.getContentLength();
            input = new BufferedInputStream(connection.getInputStream(), 8192);
            output = new FileOutputStream(zipFile);

            // "Download" and write core to storage.
            //
            long totalDownloaded = 0;
            byte[] buffer = new byte[4096];
            int countBytes = 0;
            while ((countBytes = input.read(buffer)) != -1)
            {
               totalDownloaded += countBytes;
               if (fileLen > 0)
                  publishProgress((int) (totalDownloaded * 100 / fileLen));

               output.write(buffer, 0, countBytes);
            }
            if ( zipFile.toString().endsWith(".zip") )
               unzipCore(zipFile);

            if (!zipHasInfoFile)
            { // Install info file from the same directory, if not part of the .zip
               String infoUrlPath = params[0].replace("_android.so.zip", ".info")
                                             .replace("_android.so", ".info")
                                             .replace("_android.zip", ".info");
               if ( infoUrlPath.endsWith(".info")
                    && new File(infoUrlPath.replace("file:", "")).exists() )
               {
                  final File outInfoFile
                        = new File( ctx.getApplicationInfo().dataDir + "/info/",
                                   infoUrlPath.substring(infoUrlPath.lastIndexOf('/') + 1) );
                  url = new URL(infoUrlPath);
                  connection = (URLConnection) url.openConnection();
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
         catch (IOException ignored)
         {
            // Can't really do anything to recover.
         }
         finally
         {
            try
            {
               if (output != null)
                  output.close();

               if (input != null)
                  input.close();
            }
            catch (IOException ignored)
            {
            }

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
         
         // Invoke callback to update the installed cores list.
         coreDownloadedListener.onCoreDownloaded();
         
         Toast.makeText(getActivity(), (this.coreName.isEmpty() ? "Core" : this.coreName) + " Installed.", Toast.LENGTH_LONG).show();
      }
   }

   private static void unzipCore(File zipFile)
   {
      ZipInputStream zis = null;

      try
      {
         zis = new ZipInputStream(new FileInputStream(zipFile));
         ZipEntry entry = zis.getNextEntry();

         while (entry != null)
         {
            File file;
            if (entry.getName().endsWith(".so"))
               file = new File(zipFile.getParent(), entry.getName());
            else if (entry.getName().endsWith(".info"))
            {
               file = new File(zipFile.getParent().replace("/cores", "/info"), entry.getName());
               zipHasInfoFile = true;
            }
            else {zis.getNextEntry(); continue;}

            FileOutputStream fos = new FileOutputStream(file);
            int len;
            byte[] buffer = new byte[4096];
            while ((len = zis.read(buffer)) != -1)
               fos.write(buffer, 0, len);
            fos.close();

            entry = zis.getNextEntry();
         }
      }
      catch (IOException ignored)
      {
         // Can't do anything.
      }
      finally
      {
         try
         {
            if (zis != null)
            {
               zis.closeEntry();
               zis.close();
            }
         }
         catch (IOException ignored)
         {
            // Can't do anything
         }

         zipFile.delete();
      }
   }
   
   // This will be the handler for long clicks on individual list items in this ListFragment.
   public boolean RemoveCore(int position)
   {
      final DownloadableCore core = adapter.getItem(position);
      final String corePath = core.getCoreURL().replace("file:", "");
      final String corePrefix = corePath.replace("_android.so.zip", "")
                                        .replace("_android.zip", "")
                                        .replace("_android.so", "");
      
      // Begin building the AlertDialog
      final AlertDialog.Builder alert = new AlertDialog.Builder(getActivity());
      alert.setTitle(R.string.confirm_title);
      alert.setMessage("Remove " + core.getCoreName() + " backup?");
      alert.setNegativeButton(R.string.no, null);
      alert.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener()
      {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Attempt to remove backup files
            if (new File(corePath).delete())
            {
               // remove info file if no other core files are present
               if ( !new File(corePrefix + "_android.so.zip").exists()
                    && !new File(corePrefix + "_android.so").exists()
                    && !new File(corePrefix + "_android.zip").exists() )
               {
                  new File(corePrefix + ".info").delete();
               }
               
               Toast.makeText(getActivity(), "Backup Core Removed.", Toast.LENGTH_LONG).show();
               adapter.remove(core);
               adapter.notifyDataSetChanged();
            }
            else // Failed to delete.
            {
               Toast.makeText(getActivity(), "Failed to remove backup core.", Toast.LENGTH_LONG).show();
            }
         }
      });
      alert.show();

      return true;
   }
}
