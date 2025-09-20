package com.retroarch.browser.coremanager.fragments;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import androidx.fragment.app.ListFragment;
import android.util.Log;
import android.util.Pair;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.ListView;

import com.retroarch.browser.DarkToast;
import com.retroarch.browser.ModuleWrapper;
import com.retroarch.browser.coremanager.CoreManagerActivity;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarchlite.BuildConfig;
import com.retroarchlite.R;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LOCKED;
import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
import static com.retroarch.browser.coremanager.CoreManagerActivity.getTitlePair;
import static com.retroarch.browser.coremanager.CoreManagerActivity.infoBasename;

/**
 * {@link ListFragment} that is responsible for showing
 * cores that are able to be downloaded or are not installed.
 */
public final class DownloadableCoresFragment extends ListFragment
{
   // List of TODOs.
   // - Eventually make the core downloader capable of directory-based browsing from the base URL.
   // - Allow for 'repository'-like core downloading.
   // - Clean this up a little better. It can likely be way more organized.
   // - Use a loading wheel when core retrieval is being done. User may think something went wrong otherwise.
   // - Should probably display a dialog or a toast message when the Internet connection process fails.

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

   public static final String BUILDBOT_BASE_URL = "http://buildbot.libretro.com";
   public static String BUILDBOT_CORE_URL_ARM = BUILDBOT_BASE_URL + "/nightly/android/latest/armeabi-v7a/";
   public static String BUILDBOT_CORE_URL_INTEL = BUILDBOT_BASE_URL + "/nightly/android/latest/x86/";
   public static final String BUILDBOT_INFO_URL = BUILDBOT_BASE_URL + "/assets/frontend/info.zip";
   public static final String INFO_NAME = "Core Info";
   public static final int TIMEOUT_MS = 4000;
   
   protected static OnCoreDownloadedListener coreDownloadedListener = null;
   public static ArrayList<DownloadableCore> coreList = null;
   public static DownloadableCoresAdapter dlcAdapter = null;
   private static int numInfoFilesMissing = 0;
   private static boolean infoDownloadAttempted = false;

   @Override
   public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
   {
      super.onCreateView(inflater, container, savedInstanceState);
      final ListView listView = (ListView) inflater.inflate(R.layout.coremanager_listview, container, false);
      registerForContextMenu(listView);

      dlcAdapter = new DownloadableCoresAdapter(getActivity(), android.R.layout.simple_list_item_2);
      dlcAdapter.setNotifyOnChange(true);
      listView.setAdapter(dlcAdapter);

      if (coreList == null)
         getCoreList(getActivity());
      else
      {
         dlcAdapter.clear();
         dlcAdapter.addAll(coreList);
      }

      coreDownloadedListener = (OnCoreDownloadedListener) getActivity();

      return listView;
   }

   public static void getCoreList(Context ctx)
   {
      if (BuildConfig.APPLICATION_ID.contains("64"))
      {
         BUILDBOT_CORE_URL_ARM = BUILDBOT_CORE_URL_ARM.replace("armeabi-v7a","arm64-v8a");
         BUILDBOT_CORE_URL_INTEL = BUILDBOT_CORE_URL_INTEL.replace("x86", "x86_64");
      }

      coreList = new ArrayList<>();
      new PopulateCoresListOperation(ctx).execute();
   }
   
   @Override
   public void onListItemClick(final ListView lv, final View v, final int position, final long id)
   {
      super.onListItemClick(lv, v, position, id);
      final DownloadableCore core = (DownloadableCore) lv.getItemAtPosition(position);
      final boolean isInfo = core.getCoreName().isEmpty();

      // Exit with toast if remote crc matches local file
      String localPath = getContext().getApplicationInfo().dataDir
            + "/cores/" + core.getShortURLName().replace(".zip", "");
      if (!isInfo && new File(localPath).exists()
            && core.getFileCrc32(localPath) == core.getCrc32())
      {
         DarkToast.makeText(getActivity(),
               "Latest version already installed: " + core.getCoreName());
         return;
      }

      // Prompt the user for confirmation on downloading the core.
      AlertDialog.Builder notification = new AlertDialog.Builder(getActivity());
      notification.setMessage(String.format(getString(R.string.download_core_confirm_msg),
            isInfo ? INFO_NAME : core.getCoreName()));
      notification.setTitle(R.string.confirm_title);
      notification.setNegativeButton(R.string.no, null);
      notification.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Begin downloading the core.
            new DownloadCoreOperation(getContext(), core).execute();
         }
      });
      notification.show();
   }

   static protected void DownloadInfoFiles(Context ctx)
   {
      try
      {
         DownloadableCore info = new DownloadableCore("", "", BUILDBOT_INFO_URL, false);
         new DownloadCoreOperation(ctx, info).execute();
      }
      catch (Exception ignored) {}
      infoDownloadAttempted = true;
   }

   @Override
   public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo)
   {
      super.onCreateContextMenu(menu, v, menuInfo);
      
      menu.setHeaderTitle(R.string.downloadable_cores_ctx_title);

      MenuInflater inflater = getActivity().getMenuInflater();
      inflater.inflate(R.menu.downloadable_cores_context_menu, menu);

      SharedPreferences prefs = UserPreferences.getPreferences(getContext());
      boolean sortBySys = prefs.getBoolean("sort_dl_cores_by_system", true);

      menu.findItem(R.id.sort_by_system_ctx_item).setVisible(!sortBySys);
      menu.findItem(R.id.sort_by_name_ctx_item).setVisible(sortBySys);
   }

   @Override
   public boolean onContextItemSelected(MenuItem item)
   {
      final AdapterContextMenuInfo info = (AdapterContextMenuInfo)item.getMenuInfo();

      switch (item.getItemId())
      {
         case R.id.view_core_info_ctx_item:
         {
            final String fakePath = getActivity().getApplicationInfo().dataDir + "/cores/"
                  + dlcAdapter.getItem(info.position).getShortURLName();
            final ModuleWrapper core = new ModuleWrapper(getActivity(), fakePath,
                  false, false);

            CoreInfoFragment cif = CoreInfoFragment.newInstance(core);
            cif.show(getFragmentManager(), "cif");
            return true;
         }
         case R.id.sort_by_name_ctx_item:
         {
            reSortCores(false);
            return true;
         }
         case R.id.sort_by_system_ctx_item:
         {
            reSortCores(true);
            return true;
         }
         
         default:
            return super.onContextItemSelected(item);
      }
   }
   
   public void reSortCores(boolean sortBySys)
   {
      if (dlcAdapter == null || getContext() == null)
         return;

      for (DownloadableCore core : coreList)
         core.setSortBySys(sortBySys);

      Collections.sort(coreList);
      dlcAdapter.clear();
      dlcAdapter.addAll(coreList);
      
      SharedPreferences.Editor editor = UserPreferences.getPreferences(getContext()).edit();
      editor.putBoolean("sort_dl_cores_by_system", sortBySys);
      editor.apply();
   }

   public void refreshCores()
   {
      if (dlcAdapter == null || getContext() == null)
         return;

      SharedPreferences prefs = UserPreferences.getPreferences(getContext());
      boolean sortBySys = prefs.getBoolean("sort_dl_cores_by_system", true);

      final ArrayList<DownloadableCore> cores = coreList;
      coreList = new ArrayList<>();
      coreList.add(new DownloadableCore("", "", BUILDBOT_INFO_URL, false));

      for (int i = 1; i < cores.size(); i++) // Skip info.zip
      {
         DownloadableCore core = cores.get(i);

         String fileName = core.getShortURLName();
         String infoPath = getContext().getApplicationInfo().dataDir + "/info/"
               + infoBasename((fileName));

         Pair<String,String> pair = getTitlePair(infoPath); // (name,mfr+system)
         coreList.add(new DownloadableCore(
               pair.first, pair.second, core.getCoreURL(), core.getCrc32(), sortBySys));
      }

      cores.clear();
      Collections.sort(coreList);
      dlcAdapter.clear();
      dlcAdapter.addAll(coreList);
   }

   /**
    * Async event responsible for populating the Downloadable Cores list.
    */
   private static final class PopulateCoresListOperation extends AsyncTask<Void, Void, ArrayList<DownloadableCore>>
   {
      private final Context context;

      /**
       * Constructor
       *
       * @param context The current {@link Context}.
       */
      public PopulateCoresListOperation(Context context)
      {
         this.context = context;
      }

      private HttpURLConnection getConnection(String buildbotURL)
      {
         HttpURLConnection conn = null;
         final String[] indexes = {".index-extended", ".index"};

         try
         {
            for (String index : indexes)
            {
               conn = (HttpURLConnection) new URL(buildbotURL + index).openConnection();
               conn.setConnectTimeout(TIMEOUT_MS);
               conn.setReadTimeout(TIMEOUT_MS);
               conn.connect();
               if (conn.getResponseCode() == HttpURLConnection.HTTP_OK)
                  return conn;
               Log.i("IndexDownload", "HTTP response code not OK. Response code: "
                     + conn.getResponseCode());
            }
         }
         catch (IOException e) {
            Log.e("getConnection", e.toString());
         }

         return conn;
      }

      @Override
      protected ArrayList<DownloadableCore> doInBackground(Void... params)
      {
         final ArrayList<DownloadableCore> downloadableCores = new ArrayList<>();
         SharedPreferences prefs = UserPreferences.getPreferences(context);
         boolean sortBySys = prefs.getBoolean("sort_dl_cores_by_system", true);

         try
         {
            String buildbotURL = (Build.CPU_ABI.startsWith("arm") ?
               BUILDBOT_CORE_URL_ARM : BUILDBOT_CORE_URL_INTEL);
            HttpURLConnection conn = getConnection(buildbotURL);

            // First entry is option to update info files
            downloadableCores.add(new DownloadableCore("", "",
                  BUILDBOT_INFO_URL, false));

            BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream()));

            for (String line; (line = br.readLine()) != null;)
            {
               if (line.trim().isEmpty() || line.startsWith("."))
                  continue;
               String[] tokens = line.split(" ", 3);
               String fileName = tokens.length < 3 ? line : tokens[2];
               if (fileName.startsWith("."))
                  continue;

               String crcStr   = tokens.length < 3 ? "0" : tokens[1];
               String coreURL  = buildbotURL + fileName;
               String infoPath = context.getApplicationInfo().dataDir + "/info/"
                     + infoBasename(fileName);

               Pair<String,String> pair = getTitlePair(infoPath); // (name, mfr+system)
               if (!new File(infoPath).exists())
                  numInfoFilesMissing++;

               long crc;
               try {
                  crc = Long.parseLong(crcStr.replaceFirst("^0x", ""), 16);
               } catch (NumberFormatException e) { crc = 0;}

               downloadableCores.add(new DownloadableCore(pair.first, pair.second,
                     coreURL, crc, sortBySys));
            }

            Collections.sort(downloadableCores);
            br.close();
         }
         catch (IOException e)
         {
            Log.e("PopulateCoresListOp", e.toString());
         }

         return downloadableCores;
      }

      @Override
      protected void onPostExecute(ArrayList<DownloadableCore> result)
      {
         super.onPostExecute(result);

         if (result.isEmpty())
            DarkToast.makeText(context, R.string.download_core_list_error);
         else
         {
            coreList = result;
            if (dlcAdapter != null)
            {
               dlcAdapter.clear();
               dlcAdapter.addAll(result);
            }

            if (numInfoFilesMissing > result.size()/2 && !infoDownloadAttempted)
               DownloadInfoFiles(context);
            numInfoFilesMissing = 0;
         }
      }
   }

   public static final class DownloadCoreOperation extends AsyncTask<String, Integer, Void>
   {
      private final ProgressDialog dlg;
      private final Context ctx;
      private final String coreName;
      private final String urlPath;
      private final String destDir;
      private final File destFile;
      
      /**
       * Constructor
       *
       * @param ctx  The current {@link Context}.
       * @param core Object describing the core to download.
       */
      public DownloadCoreOperation(Context ctx, DownloadableCore core)
      {
         final boolean isInfo = core.getCoreName().isEmpty();
         this.dlg = new ProgressDialog(ctx);
         this.ctx = ctx;
         this.coreName = isInfo ? INFO_NAME : core.getCoreName();
         this.urlPath = core.getCoreURL();
         this.destDir = ctx.getApplicationInfo().dataDir + (isInfo ? "/info" : "/cores");
         this.destFile = new File(this.destDir, core.getShortURLName());

         // TODO: Handle orientation changes
         ((Activity)ctx).setRequestedOrientation(SCREEN_ORIENTATION_LOCKED);
      }

      @Override
      protected void onPreExecute()
      {
         super.onPreExecute();

         dlg.setMessage(String.format(ctx.getString(R.string.downloading_msg), coreName));
         dlg.setCancelable(true);
         dlg.setCanceledOnTouchOutside(false);
         dlg.setIndeterminate(false);
         dlg.setMax(100);
         dlg.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
         dlg.setProgressNumberFormat(null);
         dlg.show();
      }
      
      @Override
      protected Void doInBackground(String... params)
      {
         InputStream is = null;
         OutputStream os = null;
         HttpURLConnection conn = null;

         try
         {
            URL url = new URL(urlPath);
            conn = (HttpURLConnection) url.openConnection();
            conn.setConnectTimeout(TIMEOUT_MS);
            conn.setReadTimeout(TIMEOUT_MS);
            conn.connect();

            if (conn.getResponseCode() != HttpURLConnection.HTTP_OK)
            {
               Log.i("DownloadCoreOperation", "HTTP response code not OK. Response code: "
                     + conn.getResponseCode());
               return null;
            }

            // Set up the streams
            final int fileLen = conn.getContentLength();
            is = new BufferedInputStream(conn.getInputStream(), 65536);
            os = new FileOutputStream(destFile);

            // Download and write to storage.
            long downloaded = 0;
            byte[] buffer = new byte[65536];
            int bufLen;
            while ((bufLen = is.read(buffer)) != -1)
            {
               if (!dlg.isShowing())
               {
                  cancel(true);
                  break;
               }

               downloaded += bufLen;
               if (fileLen > 0)
                  publishProgress((int) (downloaded * 100 / fileLen));

               os.write(buffer, 0, bufLen);
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
               if (os != null)
                  os.close();

               if (is != null)
                  is.close();
            }
            catch (IOException ignored)
            {
            }

            if (conn != null)
               conn.disconnect();
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
    
         if (dlg.isShowing())
            dlg.dismiss();

         if (destFile.exists() && destFile.getName().endsWith(".zip"))
            new UnzipCoreOperation(this.ctx, coreName, destFile.getPath(), destDir, true).execute();
         else
            ((Activity)ctx).setRequestedOrientation(SCREEN_ORIENTATION_UNSPECIFIED);
      }

      @Override
      protected void onCancelled(Void result)
      {
         if (destFile.exists())
            destFile.delete();

         DarkToast.makeText(ctx, coreName + " download canceled.");

         ((Activity)ctx).setRequestedOrientation(SCREEN_ORIENTATION_UNSPECIFIED);
      }
   }

   static final class UnzipCoreOperation extends AsyncTask<String, Integer, Void>
   {
      private final ProgressDialog dlg;
      private final Context ctx;
      private final File zipFile;
      private final String destDir;
      private final String coreName;
      private final boolean isInfo;
      private final boolean isDownload;
      private boolean isUpdate = false;

      /**
       * Constructor
       *
       * @param ctx        The current {@link Context}.
       * @param coreName   The name of the zipped core.
       * @param zipPath    Full path of the file to unzip.
       * @param destDir    Destination directory for the inflated core.
       * @param isDownload True if @zipPath was downloaded and should be deleted.
       */
      public UnzipCoreOperation(Context ctx, String coreName, String zipPath, String destDir,
                                boolean isDownload)
      {
         this.dlg = new ProgressDialog(ctx);
         this.ctx = ctx;
         this.zipFile = new File(zipPath);
         this.destDir = destDir;
         this.isDownload = isDownload;
         this.coreName = coreName;
         this.isInfo = coreName.equals(INFO_NAME);

         // TODO: Handle orientation changes
         ((Activity)ctx).setRequestedOrientation(SCREEN_ORIENTATION_LOCKED);
      }

      @Override
      protected void onPreExecute()
      {
         super.onPreExecute();

         dlg.setMessage(String.format(ctx.getString(R.string.extracting_msg), coreName));
         dlg.setCancelable(false);
         dlg.setCanceledOnTouchOutside(false);
         dlg.setIndeterminate(false);
         dlg.setMax(100);
         dlg.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
         dlg.setProgressNumberFormat(null);
         dlg.show();
      }

      @Override
      protected Void doInBackground(String... params)
      {
         FileOutputStream fos;
         FileInputStream fis;
         ZipInputStream zis = null;
         ZipEntry entry;
         File file;
         byte[] buffer = new byte[65536];
         long zipLen = zipFile.length();
         int readLen;

         try
         {
            fis = new FileInputStream(zipFile);
            zis = new ZipInputStream(fis);
            entry = zis.getNextEntry();

            while (entry != null)
            {
               String name = entry.getName();

               // Change destination if .info bundled with core
               if (!isInfo && name.endsWith(".info"))
                  file = new File(destDir.replaceFirst("/cores", "/info"), name);
               else
               {
                  file = new File(destDir, name);
                  isUpdate = isUpdate || file.exists();
               }

               if (name.endsWith(".so") || name.endsWith(".info"))
               {
                  fos = new FileOutputStream(file);
                  while ((readLen = zis.read(buffer)) != -1)
                  {
                     fos.write(buffer, 0, readLen);
                     publishProgress((int) (fis.getChannel().position() * 100 / zipLen));
                  }
                  fos.close();
               }

               zis.closeEntry();
               entry = zis.getNextEntry();
            }
         }
         catch (IOException ignored)
         {
            // Can't do anything
         }
         finally
         {
            try
            {
               if (zis != null)
                  zis.close();
            }
            catch (IOException ignored)
            {
               // Can't do anything
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

         if (isDownload)
            zipFile.delete();

         // Invoke callback to update the installed cores list.
         if (coreDownloadedListener != null)
            coreDownloadedListener.onCoreDownloaded();

         if (dlg.isShowing())
            dlg.dismiss();

         if (isInfo)
            CoreManagerActivity.downloadableCoresFragment.refreshCores();

         DarkToast.makeText(ctx,
               coreName + ((isDownload && isUpdate) ? " updated." : " installed."));

         ((Activity)ctx).setRequestedOrientation(SCREEN_ORIENTATION_UNSPECIFIED);
      }
   }
}
