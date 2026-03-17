$fn = 240;

bearing_d = 22.3;
shaft_d = 8;

h = 7;
h1 = 5;

t1=1;
t2=1.5;

key_r=.9;

module maze1() {
polygon([[bearing_d/2  , 0],
         [bearing_d/2-2, 0],
         [bearing_d/2-3, .7],
         [shaft_d/2+t1, .7],
         [shaft_d/2+t1,   h],
         [shaft_d/2+t1+1, h],
         [shaft_d/2+t1+1, 1.5],
         [bearing_d/2-t1, 1.5],
         [bearing_d/2-t1, h1],
         [bearing_d/2, h1],
         [bearing_d/2+1.2, h1],
         [bearing_d/2+1.2, 3],
         [bearing_d/2, 3]
    ]);
}

p = h1+t2;
module maze2() {
    r1 = bearing_d/2+1.2;
    arc = [for (i=[18:90]) [r1*cos(90-i), .6*r1*sin(90-i)+h1+t2]];
polygon(concat(
    [//[bearing_d/2, 7],
     [bearing_d/2-t1-t2, p],
     [bearing_d/2-t1-t2, 1.5+t2],
     [shaft_d/2+1+t1+t2, 1.5+t2],
     [shaft_d/2+1+t1+t2, h+t2],
     [shaft_d/2, h+t2],
     [shaft_d/2, h+2],
    ]
    //);
    , arc));
}

module drain() {
 linear_extrude(6,twist=270) translate([1,0])
    circle(1); 
}


if(0) {
    maze1();
    maze2();
} else {
 if(0)
rotate_extrude()
    maze1();
if(1)    
translate([40, 0, 0])    
    difference() {
        rotate_extrude()
            maze2();
        cylinder(r=shaft_d/2, h=2*h);
        for(i=[0:3]) {
            rotate(360/3*i+60)
            translate([shaft_d/2+2,0,h+1])
                drain();
        }
        translate([0,0,h+4])
        rotate([0,90,0])
        cylinder(r=key_r, h=bearing_d/2);
    }
}

