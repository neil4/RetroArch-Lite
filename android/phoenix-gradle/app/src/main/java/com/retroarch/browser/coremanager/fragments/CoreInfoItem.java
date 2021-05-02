package com.retroarch.browser.coremanager.fragments;

import android.text.TextUtils;

import java.util.List;

/**
 * Represents a single list item within the CoreInfoFragment.
 */
public final class CoreInfoItem
{
   private final String title;
   private final String subtitle;

   /**
    * Constructor
    *
    * @param title    Title of the item within the core info list.
    * @param subtitle Subtitle of the item within the core info list.
    */
   public CoreInfoItem(String title, String subtitle)
   {
      this.title = title;
      this.subtitle = subtitle;
   }

   /**
    * Constructor.
    * <p>
    * Allows for creating a subtitle out of multiple strings.
    *
    * @param title    Title of the item within the core info list.
    * @param subtitle List of strings to add to the subtitle section of this item.
    */
   public CoreInfoItem(String title, List<String> subtitle)
   {
      this.title = title;
      this.subtitle = TextUtils.join(", ", subtitle);
   }

   /**
    * Gets the title of this CoreInfoItem.
    *
    * @return the title of this CoreInfoItem.
    */
   public String getTitle()
   {
      return title;
   }

   /**
    * Gets the subtitle of this CoreInfoItem.
    *
    * @return the subtitle of this CoreInfoItem.
    */
   public String getSubtitle()
   {
      return subtitle;
   }
}
