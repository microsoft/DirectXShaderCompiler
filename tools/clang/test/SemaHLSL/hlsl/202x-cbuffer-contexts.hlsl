// RUN: %dxc -T lib_6_9 -verify -HV 202x %s

cbuffer A { // expected-note{{declared here}}
  int Y;
  // expected-error@+1 {{unsupported declaration 'N' in cbuffer declaration}}
  namespace N {
  }
  float4 F5;
}

cbuffer A { // expected-note{{declared here}}
  // expected-error@+1 {{unsupported declaration 'Nested' in cbuffer declaration}}
  cbuffer Nested {
  }
}

tbuffer TB { // expected-note{{declared here}}
  // expected-error@+1 {{unsupported declaration 'NS' in tbuffer declaration}}
  namespace NS {}
}

tbuffer TB2 { // expected-note{{declared here}}
  // expected-error@+1{{unsupported declaration 'CB2' in tbuffer declaration}}
  cbuffer CB2 {
    int X;
  }
}

namespace Valid {
  cbuffer CBValid {
    int CompletelyFine;
  }

  tbuffer TBValid {
    float StillFine;
  }

  cbuffer GoingOffTheRails { // expected-note{{declared here}}
    // expected-error@+1{{unsupported declaration 'NotCool' in cbuffer declaration}}
    tbuffer NotCool {
      ; // even if it is empty...
    }
  }

  cbuffer GoingOffTheRailsAgain { // expected-note{{declared here}}
    // expected-error@+1{{unsupported declaration 'StillNotCool' in cbuffer declaration}}
    tbuffer StillNotCool { // expected-note{{declared here}}
      // expected-error@+1{{unsupported declaration 'Turtles' in tbuffer declaration}}
      cbuffer Turtles { // expected-note{{declared here}}
        // expected-error@+1{{unsupported declaration 'AllTheWayDown' in cbuffer declaration}}
        tbuffer AllTheWayDown {
          ;
        }
      }
    }
  }
}
