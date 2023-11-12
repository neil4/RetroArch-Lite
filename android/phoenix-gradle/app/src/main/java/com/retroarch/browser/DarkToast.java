package com.retroarch.browser;

import android.content.Context;
import android.graphics.PorterDuff;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

public class DarkToast
{
   public static void makeText(Context ctx, CharSequence text)
   {
      Toast toast = Toast.makeText(ctx, text, Toast.LENGTH_LONG);
      View view = toast.getView();
      TextView txtView = view.findViewById(android.R.id.message);

      view.getBackground().setColorFilter(0x60000000, PorterDuff.Mode.SRC_IN);
      txtView.setTextColor(0xFFFFFFFF);
      toast.show();
   }

   public static void makeText(Context ctx, int res)
   {
      makeText(ctx, ctx.getText(res));
   }
}