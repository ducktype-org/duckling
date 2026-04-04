# Running Jupyter Notebooks

Below is the full instruction on how to enable Jupyter Notebook support for the Duckling language:

1. In the **ret_duckling_kernel.py** file, change the **bin_path** variable to the absolute path of your **dev/build/bin** directory.
2. In **kernel.json** change paths to your python interpreter binary and to your **ret_duckling_kernel.py**
3. If you have ever run jupyter you should have directory **.local/share/jupyter** - if not, first install the jupyter itself. Then create directory **.local/share/jupyter/kernels** and inside it, create folder named **duckling**.
4. Inside this new folder, create a **kernel.json** file. Its content should be identical to the provided template file, with the exception that the second argument must be the absolute path to **duck_kernel.py** on your specific machine(it would be even better if you move **duck_kernel.py** to just created **duckling** folder).


### Coloring
Optionally, if you want to enable syntax highlighting in jupyter - then just run:
```pip install jupyterlab-duckling-syntax-highlighting```
in your console(inside env that runs jupyter).

### Docs links
[Duckling docs](https://docs.duckling.pl/duckling/introduction/index.html) \
[Syntax highlighting plugin](https://pypi.org/project/jupyterlab-duckling-syntax-highlighting/)
