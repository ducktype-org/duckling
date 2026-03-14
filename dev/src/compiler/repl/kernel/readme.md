# Running Jupyter Notebooks

Below is the full instruction on how to enable Jupyter Notebook support for the Duckling language:

- In the **ret_duckling_kernel.py** file, change the **bin_path** variable to the absolute path of your **dev/build/bin** directory.
- In kernel.json change paths to your python interpreter binary and to your duckling project location
- Next, create a folder named **duckling** within .local/share/jupyter/kernels.
- Inside this new folder, create a **kernel.json** file. Its content should be identical to the provided template file, with the exception that the second argument must be the absolute path to ret_duck_kernel.py on your specific machine.


### Coloring
Optionally, if you want to enable syntax highlighting in jupyter - then just run:
```pip install jupyterlab-duckling-syntax-highlighting==0.1.0```
in your console(inside env that runs jupyter)