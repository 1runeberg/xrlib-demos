package dev.xrnative.checkxr;

import android.annotation.TargetApi;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;

public class MainActivity extends android.app.NativeActivity
{
  static
  {
    System.loadLibrary("xrlib");
    System.loadLibrary("checkxr");
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
  }

}
