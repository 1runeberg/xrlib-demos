package dev.xrnative.inputxr;

import android.annotation.TargetApi;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;

public class MainActivity extends android.app.NativeActivity
{
  static
  {
    System.loadLibrary("xrlib");
    System.loadLibrary("inputxr");
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
  }

}
