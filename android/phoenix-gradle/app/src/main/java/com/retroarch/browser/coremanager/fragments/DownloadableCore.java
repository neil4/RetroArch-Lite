package com.retroarch.browser.coremanager.fragments;

import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.net.URL;
import java.util.zip.CRC32;

/**
 * Represents a core that can be downloaded.
 */
final class DownloadableCore implements Comparable<DownloadableCore>
{
   private final String coreName;
   private final String systemName;
   private String title;
   private String subTitle;
   private final String coreURL;
   private final String shortURL;
   private final long crc32;
      
   /**
    * Constructor
    *
    * @param coreName Name of the core.
    * @param systemName Name of the system emulated by the core
    * @param coreURL  URL to this core.
    * @param sortBySys Whether to use systemName or coreName as title
    */
   public DownloadableCore(String coreName, String systemName, String coreURL, boolean sortBySys)
   {
      this.coreName   = coreName;
      this.systemName = systemName;
      this.coreURL    = coreURL;
      this.shortURL   = coreURL.substring(coreURL.lastIndexOf('/') + 1);
      this.crc32      = 0;
      setSortBySys(sortBySys);
   }

   public DownloadableCore(String coreName, String systemName, String coreURL, long crc32, boolean sortBySys)
   {
      this.coreName   = coreName;
      this.systemName = systemName;
      this.coreURL    = coreURL;
      this.shortURL   = coreURL.substring(coreURL.lastIndexOf('/') + 1);
      this.crc32      = crc32;
      setSortBySys(sortBySys);
   }

   /**
    * Gets the name of this core.
    *
    * @return The name of this core.
    */
   public String getCoreName()
   {
      return coreName;
   }
   
   public String getSystemName()
   {
      return systemName;
   }

   public String getTitle()
   {
      return title;
   }

   public String getSubTitle()
   {
      return subTitle;
   }

   public long getCrc32()
   {
      return crc32;
   }

   public void setSortBySys(boolean sortBySys)
   {
      if (sortBySys)
      {
         title = systemName;
         subTitle = coreName;
      }
      else
      {
         title = coreName;
         subTitle = systemName;
      }
   }

   /**
    * Gets the URL to download this core.
    *
    * @return The URL to download this core.
    */
   public String getCoreURL()
   {
      return coreURL;
   }

   public String getFilePath()
   {
      try {
         return new File(new URL(coreURL).toURI()).getPath();
      }
      catch (Exception e) {
         return null;
      }
   }

   /**
    * Gets the short URL name of this core.
    * <p>
    * e.g. Consider the url: www.somesite/somecore.zip.
    *      This would return "somecore.zip"
    *
    * @return the short URL name of this core.
    */
   public String getShortURLName()
   {
      return shortURL;
   }
   
   @Override
   public int compareTo(DownloadableCore other)
   {
      return title.compareToIgnoreCase(other.title);
   }

   public static long getFileCrc32(String filePath)
   {
      CRC32 crc = new CRC32();
      byte[] buf = new byte[16384];

      try (FileInputStream fis = new FileInputStream(filePath))
      {
         int numRead;
         while ((numRead = fis.read(buf)) != -1)
            crc.update(buf, 0, numRead);
      }
      catch (IOException e)
      {
         Log.e("getFileCrc32", e.toString());
      }

      return crc.getValue();
   }
}
