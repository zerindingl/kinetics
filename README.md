# kinetics: C++ library for Control, and Planning.


C++ library implementation of [_Modern Robotics: Mechanics, Planning, and Control_](https://modernrobotics.org) (Kevin Lynch and Frank Park, Cambridge University Press 2017).

functional implementation is available in Python, Matlab, Mathematica: [ Modern Robotics ](https://github.com/NxRLab/ModernRobotics/)

# required libraries:
- make sure to install eigens library
- install algebra library see the instructions[Algebra](http://github.com/ertosns/algebra.git)
- igl ....


# installation

```console
user@name:~$ . ./install.sh

```

# verify kinetics is working
```console
user@name:~$ g++ exercises.cpp  -I /usr/include/eigen3 -lpthread -lalgebra -lkinetics
```
