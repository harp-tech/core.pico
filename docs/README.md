## Prerequisites
[Doxygen](https://doxygen.nl/) and [Graphviz](https://graphviz.org/) must first be installed.

To build the documentation, invoke:
````
doxygen
````

## Deployment

Documentation will automatically publish to GitHub Pages when:

* A release is made.
* A manual workflow dispatch is initiated and publishing is enabled.
* Any push to any branch is made when `CONTINUOUS_DOCUMENTATION` is enabled (intended for use in forks for development purposes).