# game-interface
game interface directx11 hlsl


//*, this is quad/rect-based interface, all quads are stored in either a static class or dynamic class, search support: bvh/avl
//*, dynamic class mainly is used for editbox, static class is for all others.
//*, vertices in quad are pre-tranformed, so, rendering is the fastest, moving is relatively slow, because every vertex needs to be
//	transformed on CPU side then update to GPU, but, it's fine, because transforming is a really simple operation(just an addition)
//*, 

![alt text](https://github.com/dong-zhan/game-interface/blob/master/4.jpg)
