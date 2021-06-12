package com.retroarch.browser.coremanager.fragments;

/**
 * Represents a core that can be downloaded.
 */
final class DownloadableCore implements Comparable<DownloadableCore>
{
   private final String coreName;
   private final String systemName;
   private final String title;
   private final String subTitle;
   private final String coreURL;
   private final String shortURL;
      
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
      this.title      = sortBySys ? systemName : coreName;
      this.subTitle   = sortBySys ? coreName   : systemName;
      this.coreURL  = coreURL;
      this.shortURL = coreURL.substring(coreURL.lastIndexOf('/') + 1);
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

   /**
    * Gets the URL to download this core.
    *
    * @return The URL to download this core.
    */
   public String getCoreURL()
   {
      return coreURL;
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
}
