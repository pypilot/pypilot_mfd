$fn = 60;

len = 10;
module head() {
  rotate(90, [1, 0, 0])
      difference() {
         scale([1, .5, 1]) {
         cylinder(h=len, r1=5.6, r2=5);
          translate([0,0,len])
          sphere(5);
          }
         translate([0, 0, -1])
            cylinder(h=len, r=2.1);
      }
}

rotate([-90,0,0])
    head();
