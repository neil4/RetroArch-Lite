package com.retroarch.browser;

import java.io.File;
import java.io.IOException;

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
}
