length = 124;
width = 97;
height = 19;

thickness = 8;
ex_r = 3;
wall = 2.4;

module box() {
    difference() {
        translate([-length/2-wall,-width/2-wall,0])
        minkowski() {
            cube([length-ex_r*2+wall*2, width-ex_r*2+wall*2, height+wall*2]);
            cylinder(r=thickness+ex_r,h=bthickness);
        }
        translate([-length/2,-width/2,wall])
        minkowski() {
            cube([length-ex_r*2, width-ex_r*2, height+10]);
            cylinder(r=thickness+ex_r,h=bthickness);
        }
        
         translate([-length/2-thickness-wall-2.5, 0, height/2])
        rotate([0, -90, 0])
        rotate(90)
            linear_extrude(height = 2)
                text("pypilot", size = 10, halign = "center", valign = "center", $fn = 16);  
    }
}
box();