# 00a-the-as-level-bridge: translating gcse to matrix form

if you have just started sixth form, your mathematical toolkit consists of basic calculus ($\frac{dy}{dx}$), simultaneous equations, and 2d graphs ($y = mx + c$).

this project operates in 3d vector space with 27,000 variables. this document is a "cheat sheet" to bridge your standard AS-Level textbook into the linear algebra required for the simulator. you don't need a university degree, you just need the translation key.

---

## 1. Simultaneous Equations to Matrices (The Spreadsheet)

**Where you are:**
you know how to solve for $x$ and $y$:
$$2x + 3y = 10$$
$$4x -  y =  6$$

**The Bridge:**
in RTI, we have 28 sensors measuring signal loss ($\mathbf{y}$). and we have 27,000 voxels in a room ($\mathbf{x}$).
instead of writing 28 lines of equations, we use a single matrix equation:
$$ \mathbf{Wx} = \mathbf{y} $$

*   $\mathbf{W}$ is a massive spreadsheet (a "matrix"). it has 28 rows and 27,000 columns.
*   $\mathbf{x}$ is a single column of 27,000 numbers (the voxels).
*   $\mathbf{y}$ is a single column of 28 numbers (the measurements).

to multiply them, you take row 1 of $\mathbf{W}$, multiply it by the column $\mathbf{x}$, and add it all up. that gives you $y_1$.
this is mathematically identical to 28 massive simultaneous equations.

**The Transpose ($\mathbf{W}^T$):**
if you see $\mathbf{W}^T$, it just means "flip the spreadsheet diagonally". row 1 becomes column 1. a $28 \times 27000$ matrix becomes a $27000 \times 28$ matrix.

---

## 2. 2D Lines to 3D Rays (Parametric Vectors)

**Where you are:**
you draw lines with $y = mx + c$.

**The Bridge:**
you cannot use $y = mx + c$ in 3D. if a line goes straight up the z-axis, $x$ doesn't change, so what is the gradient $m$? it's undefined ($\infty$). 

to shoot a radio wave from Node A to Node B in 3D, we use **parametric vector equations**.
instead of relating $x$ to $y$, we introduce a slider called $\lambda$ (or $\alpha$).

let the start position be vector $\mathbf{a} = \begin{pmatrix} x_1 \\ y_1 \\ z_1 \end{pmatrix}$.
let the direction we want to travel be vector $\mathbf{d}$.

the line equation is:
$$ \mathbf{r} = \mathbf{a} + \lambda \mathbf{d} $$

*   if $\lambda = 0$, you are at the start ($\mathbf{a}$).
*   if $\lambda = 1$, you are at the end.
*   if $\lambda = 0.5$, you are exactly halfway.

in `02-forward-model.md`, Siddon's algorithm uses this exact logic. instead of checking every 1mm in the room, it just finds the exact $\lambda$ values where the line hits a voxel wall.

---

## 3. Finding the Bottom of the Bowl (Differentiation)

**Where you are:**
to find the minimum of $y = x^2 - 4x + 4$, you differentiate to $\frac{dy}{dx} = 2x - 4$.
you set it to $0$:
$$2x - 4 = 0 \implies x = 2$$
the bottom of the curve is at $x=2$.

**The Bridge:**
in RTI, we need to find the "minimum error" to guess where the human is standing. the equation isn't a 2D curve, it's a massive 27,000-dimensional bowl.

to find the bottom of a bowl, we can't use $\frac{dy}{dx}$. we use the **vector gradient**, written as $\nabla$ (nabla), or partial derivatives ($\frac{\partial}{\partial x}$).

*   $\frac{dy}{dx}$ tells you the slope on a 2D page.
*   $\nabla$ tells you the slope in every direction simultaneously. 

if you set $\nabla = \mathbf{0}$ (a vector of all zeros), you have found the exact bottom of the 27,000-dimensional bowl. this is how we mathematically calculate the perfect image of the room, which leads directly to the A-Level bridge.