package com.retroarch.browser;

import android.content.Context;
import android.os.Build;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public final class StorageInterface
{
   public static void deleteDirTree(File topDir, boolean deleteTop)
   {
      if (topDir.isDirectory())
      {
         String[] names = topDir.list();
         for (String name : names)
            deleteDirTree(new File(topDir, name), true);
      }

      if (deleteTop)
         topDir.delete();
   }

   /**
    * Gets storage volume paths, delimited by {@code delim}
    * @param ctx current {@link Context}
    * @param delim char between paths
    * @return storage volume paths
    */
   public static String getVolumePaths(Context ctx, char delim)
   {
      StringBuilder sb = new StringBuilder();

      if (Build.VERSION.SDK_INT >= 24)
      {
         StorageManager sm = (StorageManager) ctx.getSystemService(Context.STORAGE_SERVICE);
         List<StorageVolume> volList = sm.getStorageVolumes();

         for (StorageVolume vol : volList)
         {
            if (Build.VERSION.SDK_INT >= 30)
               sb.append(vol.getDirectory()).append(delim);
            else
            {
               try
               {
                  Method getPathMethod = StorageVolume.class.getDeclaredMethod("getPath");
                  sb.append(getPathMethod.invoke(vol)).append(delim);
               }
               catch (Exception e)
               {
                  Log.e("getVolumePaths", e.toString());
                  break;
               }
            }
         }
      }
      return sb.toString();
   }

   /**
    * Extracts files from a zip archive with an optional subdirectory
    * and optional set of allowed extensions.
    *
    * @param zipPath absolute path of the zip file
    * @param zipSubDir directory path to use inside zip file, or null to use root
    * @param allowedExt allowed file extensions delimited by '|', or null to allow all
    * @param destPath absolute destination directory
    * @return {@code true} if at least one file was extracted with no errors,
    *         {@code false} otherwise
    */
   public static boolean extractArchive(String zipPath, String zipSubDir,
         String allowedExt, String destPath)
   {
      if (zipSubDir == null)
         zipSubDir = "";
      if (!zipSubDir.isEmpty())
         zipSubDir += "/";

      File destDir = new File(destPath);
      if (!destDir.exists() && !destDir.mkdirs())
      {
         Log.e("extractArchive", "Failed to create destination directory");
         return false;
      }

      // Turn allowedExt into an ArrayList
      ArrayList<String> allowedExts = new ArrayList<>();
      if (allowedExt != null && !allowedExt.trim().isEmpty())
      {
         for (String ext : allowedExt.split("\\|"))
            allowedExts.add(ext.trim().toLowerCase());
      }

      boolean anyExtracted = false;

      try (ZipFile zipFile = new ZipFile(zipPath))
      {
         Enumeration<? extends ZipEntry> entries = zipFile.entries();

         while (entries.hasMoreElements())
         {
            ZipEntry entry = entries.nextElement();

            if (entry.isDirectory() || !entry.getName().startsWith(zipSubDir))
               continue;

            // Ignore disallowed extensions
            if (!allowedExts.isEmpty())
            {
               String name = entry.getName();
               int idx = name.lastIndexOf('.');
               if (idx == -1)
                  continue;
               String ext = name.substring(idx + 1).toLowerCase();
               if (!allowedExts.contains(ext))
                  continue;
            }

            // Get output path and mkdirs
            File outFile = new File(destDir, entry.getName().substring(zipSubDir.length()));
            File parent  = outFile.getParentFile();
            if (!parent.exists() && !parent.mkdirs())
            {
               Log.e("extractArchive", "Failed to create subdirectory: " + parent);
               return false;
            }

            // Extract file
            try (InputStream is = zipFile.getInputStream(entry);
                 FileOutputStream os = new FileOutputStream(outFile))
            {
               byte[] buffer = new byte[65536];
               int bufLen;
               while ((bufLen = is.read(buffer)) != -1)
                  os.write(buffer, 0, bufLen);
               anyExtracted = true;
            }
            catch (Exception e)
            {
               Log.e("extractArchive", "Extraction failed for " + entry.getName());
               return false;
            }
         } // end while
      }
      catch (Exception e)
      {
         Log.e("extractArchive", e.getMessage());
         return false;
      }

      return anyExtracted;
   }

   /**
    * Installs assets from known subdirectories in a zip file or apk.
    *
    * @param archivePath absolute path to .zip or .apk
    * @param srcBaseDir relative path in archive where asset subdirectories are
    * @param destBaseDir absolute path to destination directory
    * @param clearDestDirs {@code true} to delete all files in destination subdirectories first
    * @return {@code true} if at least one file was extracted, {@code false} otherwise
    */
   public static boolean installAssetsFromArchive(String archivePath, String srcBaseDir,
                                                  String destBaseDir, boolean clearDestDirs)
   {
      boolean success = false;

      final String[] assetDirs =
            {"overlays",      "info",     "shaders_glsl", "themes_rgui", "video_filters",
             "audio_filters", "autoconfig"};
      final String[] assetExts =
            {"cfg|png",       "info|txt", "glslp|glsl",   "cfg|png",     "filt",
             "so|dsp",        "cfg"};

      for (int i = 0; i < assetDirs.length; i++)
      {
         // Set source and destination directories to next in assetDirs
         String srcDir = (srcBaseDir != null && !srcBaseDir.isEmpty())
               ? srcBaseDir + '/' + assetDirs[i]
               : assetDirs[i];
         String destDir  = destBaseDir + '/' + assetDirs[i];
         File destDirObj = new File(destDir);

         if (clearDestDirs)
            StorageInterface.deleteDirTree(destDirObj, false);
         if (!destDirObj.exists() && !destDirObj.mkdirs())
            continue;

         success |= StorageInterface.extractArchive(archivePath, srcDir, assetExts[i], destDir);
      }

      return success;
   }
}
