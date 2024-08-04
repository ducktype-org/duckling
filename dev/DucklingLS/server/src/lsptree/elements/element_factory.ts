const DEBUG = false;

/*
This file contains impleentation of Ducling Element factories.
They are used to create Element instances from json objects provided by compiler daemon.

The idea of factory structure provides solution to a following problem.
Let's say we have class A and B extending A.

We want to provide an instance for variable of type A based on given `.json` serialized instance:

```
{
	"B":{
		"param1":...
		"param2":...
		"param3":...
	
	}
}
```
By joining factories into tree like structure it's easly possible to call factoryA to create an instance of B based on configuration.

*/ 

export class ElementException extends Error {
	constructor(message: string) {
		super(message);
		this.name = "UnknownElementException";
	}
}

interface ElementJSON<ProductType> {
	new(json: any): ProductType;
}

export interface ElementFactoryInterface {
	register(name: string, cls: ElementJSON<any>): void;
	create(json: { [key: string]: any }, allow_unknown: boolean): any | undefined;
}

export class ParentlessElementFactory<ProductType> implements ElementFactoryInterface {

	// Constructor registry.
	public registry: Map<string, ElementJSON<ProductType>> = new Map<string, ElementJSON<ProductType>>();

	constructor() {
		this.registry = new Map<string, ElementJSON<ProductType>>();
	}

	// Adds a new class to registry.
	register(name: string, cls: ElementJSON<ProductType>) {
		this.registry.set(name, cls);
	}

	// Creates new instance based on `.json` serialized instance.
	create(json: { [key: string]: any }, allow_unknown: boolean = DEBUG): ProductType | undefined {
		const keys = Object.keys(json);
		if (keys.length !== 1) {
			throw new ElementException("Invalid JSON for DucklingElement: " + JSON.stringify(json));
		}
		const name = keys[0];
		const cls = this.registry.get(name);
		if (cls === undefined) {
			console.log("encountered unknown element: " + name);
			if (allow_unknown)
				return undefined;
			else
				throw new ElementException("Unknown Element class: " + name);
		}
		return new cls(json[name]);
	}

	// Creates new instance based on `.json` serialized instance.
	createDefined(json: { [key: string]: any }): ProductType {
		const result = this.create(json, false);
		if (result === undefined) {
			throw new ElementException("Unknown Element class: " + JSON.stringify(json));
		}
		return result;
	}

	// Check if class is present in registry.
	isObjectClassValid(json: { [key: string]: any }): boolean {
		const keys = Object.keys(json);
		if (keys.length !== 1) {
			throw new ElementException("Invalid JSON for DucklingElement: " + JSON.stringify(json));
		}
		const name = keys[0];
		return this.registry.has(name);
	}
}

export class ElementFactory<ProductType, ParentFactory extends ElementFactoryInterface> extends ParentlessElementFactory<ProductType> {
	public registry: Map<string, ElementJSON<ProductType>> = new Map<string, ElementJSON<ProductType>>();
	private parentFactory: ParentFactory;

	constructor(parentFactory: ParentFactory) {
		super();
		this.parentFactory = parentFactory;
	}

	// Adds a new class to registry and parents registry.
	override register(name: string, cls: ElementJSON<ProductType>) {
		this.registry.set(name, cls);
		this.parentFactory.register(name, cls);
	}
}