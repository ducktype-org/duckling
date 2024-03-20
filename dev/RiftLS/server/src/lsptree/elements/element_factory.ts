const DEBUG = true;

export class ElementException extends Error {
	constructor(message: string) {
		super(message);
		this.name = "UnknownElementException";
	}
}

interface ElementJSON<ProductType> {
    new (json: any): ProductType;
}

export interface IntElementFactory {
    register(name: string, cls: ElementJSON<any>): void;
    create(json: {[key: string]: any}, allow_unknown: boolean): any | undefined;
}

export class ParentlessElementFactory<ProductType> implements IntElementFactory{
	public registry: Map<string, ElementJSON<ProductType>> = new Map<string, ElementJSON<ProductType>>();

	constructor() {
		this.registry = new Map<string, ElementJSON<ProductType>>();
	}

	register(name: string, cls: ElementJSON<ProductType>) {
		this.registry.set(name, cls);
	}

	create(json: {[key: string]: any}, allow_unknown: boolean=DEBUG): ProductType | undefined {
		const keys = Object.keys(json);
		if (keys.length !== 1) {
			throw new ElementException("Invalid JSON for RiftElement: " + JSON.stringify(json));
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

	createDefined(json: {[key: string]: any}): ProductType {
		const result = this.create(json, false);
		if (result === undefined) {
			throw new ElementException("Unknown Element class: " + JSON.stringify(json));
		}
		return result;
	}

	isObjectClassValid(json: {[key: string]: any}): boolean {
		const keys = Object.keys(json);
		if (keys.length !== 1) {
			throw new ElementException("Invalid JSON for RiftElement: " + JSON.stringify(json));
		}
		const name = keys[0];
		return this.registry.has(name);
	}
}

export class ElementFactory<ProductType, ParentFactory extends IntElementFactory> extends ParentlessElementFactory<ProductType>{
	public registry: Map<string, ElementJSON<ProductType>> = new Map<string, ElementJSON<ProductType>>();
	private parentFactory: ParentFactory;


	constructor(parentFactory: ParentFactory) {
		super();
		this.parentFactory = parentFactory;
	}

	override register(name: string, cls: ElementJSON<ProductType>) {
		this.registry.set(name, cls);
		this.parentFactory.register(name, cls);
	}
}