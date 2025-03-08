package com.retroarch.browser;

import android.content.Context;
import android.os.Build;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.List;

/**
 * Helper class which calls into JNI for various tasks.
 */
public final class NativeInterface
{
   static
   {
      System.loadLibrary("retroarch-jni");
   }

   // Disallow explicit instantiation.
   private NativeInterface()
   {
   }

   public static native boolean extractArchiveTo(String archive,
                                                 String subDirectory, String destinationFolder);

   public static boolean RestoreDirFromZip(final String zipPath, final String zipSubDir,
                                           final String destDir)
   {
      boolean success;
      try
      {
         File dir = new File(destDir);
         if (dir.isDirectory())
         {
            String[] names = dir.list();
            for (String name : names)
               DeleteDirTree(new File(dir, name));
         }

         success = extractArchiveTo(zipPath, zipSubDir, destDir);
         if (!success)
            throw new IOException("Failed to extract assets ...");
      }
      catch (IOException e)
      {success = false;}

      return success;
   }

   public static boolean DeleteDirTree(File topDir)
   {
      if (topDir.isDirectory())
      {
         String[] names = topDir.list();
         for (String name : names)
            DeleteDirTree(new File(topDir, name));
      }

      return topDir.delete();
   }

   /**
    * Gets storage volume paths, delimited by {@code delim}
    * @param ctx Context
    * @param delim char between paths
    * @return storage volume paths
    */
   public static String getVolumePaths(Context ctx, char delim)
   {
      String ret = "";

      if (Build.VERSION.SDK_INT >= 24)
      {
         StorageManager sm = (StorageManager) ctx.getSystemService(Context.STORAGE_SERVICE);
         List<StorageVolume> volList = sm.getStorageVolumes();

         for (StorageVolume vol : volList)
         {
            if (Build.VERSION.SDK_INT >= 30)
               ret += String.valueOf(vol.getDirectory()) + delim;
            else
            {
               try
               {
                  Method getPathMethod = StorageVolume.class.getDeclaredMethod("getPath");
                  ret += ((String) getPathMethod.invoke(vol)) + delim;
               }
               catch (Exception e)
               {
                  Log.e("getVolumePaths", e.toString());
                  break;
               }
            }
         }
      }

      return ret;
   }
}
