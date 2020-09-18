mixin template Field(string name, string type, string high, string low) {
	static if(high != low) {
		mixin("@property " ~ type ~ " " ~ name ~ "() {" ~
				type ~ " mask = " ~ "((((1<< (" ~ high ~ "-" ~ low ~ "+ 1)) - 1) <<" ~ low ~ "));" ~
				"return (raw & mask) >>" ~ low ~ ";"
				~ "}");
		mixin("@property void " ~ name ~ "(" ~ type ~ " val) {" ~
				"raw &= " ~ "(~(((1<< (" ~ high ~ "-" ~ low ~ "+ 1)) - 1) <<" ~ low ~ "));" ~
				"raw |= (val << " ~ low ~ ");"
				~ "}");
	} else {
		mixin("@property " ~ type ~ " " ~ name ~ "() {" ~
				"return (raw & (1 <<" ~ high ~ ")) != 0;"
				~ "}");
		mixin("@property void " ~ name ~ "(" ~ type ~ " val) {" ~
				"raw &= ~(1 << " ~ high ~ ");" ~
				"raw |= val << " ~ high ~ ";"
				~ "}");
	}
}

struct field {
	string name;
	string high;
	string low;
}

mixin template LoadStore() {
	mixin("import core.volatile;");
	mixin("void load() { raw = volatileLoad(addr);}");
	mixin("void store() { volatileStore(addr, raw);}");
}

mixin template BitFields(string type, T...) {
	mixin(type ~ "* addr;");
	mixin(type ~ " raw;");
	static foreach(field; T) {
		mixin Field!(field.name, type, field.high, field.low);
	}
	mixin LoadStore!();

	this(size_t a) {
		import memory.virtual;
		mixin("addr = cast(" ~ type ~ "*)" ~ "(a + MEM_PHYS_OFFSET);");
	}
}
