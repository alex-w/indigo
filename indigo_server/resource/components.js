/*
 Copyright (c) 2017 GUIMakers, s. r. o. All rights reserved.
 You can use this software under the terms of 'INDIGO Astronomy open-source license' (see LICENSE.md).
 */

Vue.component('indigo-select-item', {
	props: {
		property: Object,
		no_value: String
	},
	methods: {
		onChange: function(e) {
			var values = {};
			values[e.target.value] = true;
			changeProperty(this.property.device, this.property.name, values);
		},
		state: function() {
			return this.property.state.toLowerCase() + "-state";
		},
		none_selected: function() {
			for (var i in this.property.items) {
				if (this.property.items[i].value) return false;
			}
			return true;
		}
	},
	template: `
		<select v-if="property != null" class="custom-select m-1 w-100" :class="state()" @change="onChange">
		<template v-if="none_selected()">
			<option disabled>{{ no_value }}</option>
		</template>
		<template v-else>
			<option v-for="item in property.items" :selected="item.value" :value="item.name">
				{{ item.label }}
			</option>
		</template>
		</select>`
});

Vue.component('indigo-edit-number', {
	props: {
		property: Object,
		enabler: Object,
		name: String,
		icon: String,
		values: Array
	},
	methods: {
		change: function(value) {
			var values = {};
			if (value === "Off") {
				if (this.enabler != null) {
					for (var i in this.enabler.items) {
						var item = this.enabler.items[i];
						if (item.name == "OFF" || item.name == "DISABLED") {
							if (!item.value) {
								values[item.name] = true;
								changeProperty(this.enabler.device, this.enabler.name, values);
								return;
							}
						}
					}
				} else {
					value = 0;
				}
			} else {
				if (typeof value == "string") value = parseInt(value);
				if (this.enabler != null) {
					for (var i in this.enabler.items) {
						var item = this.enabler.items[i];
						if (item.name == "ON" || item.name == "ENABLED") {
							if (!item.value) {
								values[item.name] = true;
								changeProperty(this.enabler.device, this.enabler.name, values);
								values = {};
								break;
							}
						}
					}
				}
				values[this.name] = value;
				changeProperty(this.property.device, this.property.name, values);
			}
		},
		onChange: function(e) {
			this.change(e.target.value);
		},
		state: function() {
			return this.property.state.toLowerCase() + "-state";
		},
		value: function() {
			if (this.property == null) return null;
			if (this.enabler != null) {
				for (var i in this.enabler.items) {
					var item = this.enabler.items[i];
					if (item.value && (item.name == "OFF" || item.name == "DISABLED")) return "Off";
				}
			}
			for (var i in this.property.items) {
				var item = this.property.items[i];
				if (item.name == this.name) {
					if (this.property.perm == "ro")
						return item.value;
					return item.target;
				}
			}
			return null;
		}
	},
	template: `
		<div v-if="property != null" class="input-group p-1 w-50">
		<a class="input-group-prepend">
			<span class="input-group-text glyphicons" :class="icon + ' ' + state()"></span>
		</a>
		<input v-if="property.perm == 'ro'" readonly type="text" class="form-control input-right" :value="value()">
		<input v-else type="text" class="form-control input-right" :value="value()" @change="onChange">
		<div v-if="values != null" class="input-group-append">
			<button class="btn dropdown-toggle dropdown-toggle-split btn-outline-secondary" type="button" data-toggle="dropdown"></button>
			<div class="dropdown-menu">
				<a class="dropdown-item" href="#" v-for="value in values" @click="change(value)">{{value}}</a>
			</div>
		</div>
		</div>`
});

Vue.component('indigo-show-number', {
	props: {
		property: Object,
		enabler: Object,
		name: String,
		icon: String
	},
	methods: {
		state: function() {
			return this.property.state.toLowerCase() + "-state";
		},
		value: function() {
			if (this.property == null) return null;
			if (this.enabler != null) {
				for (var i in this.enabler.items) {
					var item = this.enabler.items[i];
					if (item.value && (item.name == "OFF" || item.name == "DISABLED")) return "Off";
				}
			}
			for (var i in this.property.items) {
				var item = this.property.items[i];
				if (item.name == this.name) return item.value;
			}
			return null;
		}
	},
	template: `
		<div v-if="property != null" class="btn-group btn-group-sm p-1 w-25">
		<button type="button" class="btn p-0 w-40" :class="state()">
			<span class="glyphicons" :class="icon + ' ' + state()" />
		</button>
		<button type="button" class="btn w-60 text-right" :class="state()">
			{{value()}}
		</button>
		</div>`
});

Vue.component('indigo-edit-text', {
	props: {
		property: Object,
		name: String,
		icon: String,
	},
	methods: {
		onChange: function(e) {
			var values = {};
			values[this.name] = e.target.value;
			changeProperty(this.property.device, this.property.name, values);
		},
		state: function() {
			return this.property.state.toLowerCase() + "-state";
		},
		item: function() {
			if (this.property == null) return null;
			for (var i in this.property.items) {
				var item = this.property.items[i];
				if (item.name == this.name) return item;
			}
			return null;
		}
	},
	template: `
		<div v-if="property != null" class="input-group p-1 w-100">
		<div class="input-group-prepend">
			<span class="input-group-text glyphicons" :class="icon + ' ' + state()"></span>
		</div>
		<input type="text" class="form-control" :value="item().value" @change="onChange">
		</div>`
});

Vue.component('indigo-stepper', {
	props: {
		property: Object,
		name: String,
		direction: Object,
		direction_left: String,
		direction_right: String
	},
	methods: {
		left: function(value) {
			var values = {};
			values[this.direction_left] = true;
			changeProperty(this.direction.device, this.direction.name, values);
			values = {};
			if (typeof value == "string") value = parseInt(value);
			values[this.name] = value;
			changeProperty(this.property.device, this.property.name, values);
		},
		right: function(value) {
			var values = {};
			values[this.direction_right] = true;
			changeProperty(this.direction.device, this.direction.name, values);
			values = {};
			if (typeof value == "string") value = parseInt(value);
			values[this.name] = value;
			changeProperty(this.property.device, this.property.name, values);
		},
		state: function() {
			return this.property.state.toLowerCase() + "-state";
		},
		value: function() {
			if (this.property == null) return null;
			for (var i in this.property.items) {
				var item = this.property.items[i];
				if (item.name == this.name) return item.value;
			}
			return null;
		}
	},
	template: `
		<div v-if="property != null" class="input-group p-1 w-50">
			<div class="input-group-prepend">
				<button class="btn glyphicons glyphicons-arrow-left" :class="state()" @click="left($($event.target).parent().next().val())" type="button"></button>
			</div>
			<input type="text" class="form-control input-right" :value="value()">
			<div class="input-group-append">
				<button class="btn glyphicons glyphicons-arrow-right" :class="state()" @click="right($($event.target).parent().prev().val())" type="button"></button>
			</div>
		</div>`
});

Vue.component('indigo-ctrl', {
	props: {
		devices: Object
	},
	methods: {
		groups: function(device) {
			var result = {};
			for (p in device) {
				var property = device[p];
				var group = result[property.group];
				if (group == null) {
					group = {};
					result[property.group] = group;
				}
				group[property.name] = property;
			}
			return result;
		},
		state: function(object) {
			if (object.state != null)
				return object.state.toLowerCase() + "-state";
			if (object.value != null)
				return object.value.toLowerCase() + "-state";
			for (p in object) {
				var property = object[p];
				if (property.name == "CONNECTION") {
					if (property.state == "Ok") {
						for (i in property.items) {
							var item = property.items[i];
							if (item.name == "CONNECTED" && item.value) {
								return "ok-state";
							}
						}
					}
					break;
				}
			}
			return "idle-state";
		},
		setSwitch: function(property, itemName, value) {
			var values = {};
			values[itemName] = value;
			changeProperty(property.device, property.name, values);
		},
		dirty: function(item) {
			if (item.newValue != null) return "dirty";
			return "";
		},
		value: function(item) {
			return item.newValue != null ? item.newValue : item.value;
		},
		newValue: function(item, value) {
			Vue.set(item, 'newValue', value);
		},
		reset: function(property) {
			for (i in property.items) {
				Vue.set(property.items[i], 'newValue', null);
			}
		},
		set: function(property) {
			var values = {};
			for (i in property.items) {
				var item = property.items[i];
				values[item.name] = item.value;
				item.newValue = null;
			}
			changeProperty(property.device, property.name, values);
		},
		openAll: function(element) {
			var header = $(element).parent();
			var body = header.next();
			header.removeClass("collapsed");
			body.addClass("show");
			$(body).find("button.collapsed").removeClass("collapsed");
			$(body).find("div.collapse").addClass("show");
		},
	},
	template: `
		<div class="accordion p-1 w-100">
		<div class="card" v-for="(device,deviceName) in devices">
			<button class="btn card-header p-2 collapsed" :class="state(device)" data-toggle="collapse" :data-target="'#' + deviceName.hashCode()" style="text-align:left"><span class="icon-indicator"></span>{{deviceName}}<span class="float-right" @click.stop="openAll($event.target)">▶▶</span></button>
			<div :id="deviceName.hashCode()" class="accordion collapse p-2">

				<div class="card" v-for="(group,groupName) in groups(device)">
					<button class="btn card-header p-2 collapsed" data-toggle="collapse" :data-target="'#' + deviceName.hashCode() + '_' + groupName.hashCode()" style="text-align:left"><span class="icon-indicator"></span>{{groupName}}<span class="float-right" @click.stop="openAll($event.target)">▶▶</span></button>
					<div :id="deviceName.hashCode() + '_' + groupName.hashCode()" class="accordion collapse p-2">

						<div class="card" v-for="(property,name) in group">
							<button class="btn card-header p-2 collapsed" :class="state(property)" @dblclick="foldAll($event.target)" data-toggle="collapse" :data-target="'#' + deviceName.hashCode() + '_' + groupName.hashCode() + '_' + name" style="text-align:left"><span class="icon-indicator"></span>{{property.label}}<small class="float-right">{{name}}</small></button>
							<div :id="deviceName.hashCode() + '_' + groupName.hashCode() + '_' + name" class="collapse card-block p-2 bg-light">
								<form class="m-0">
									<template v-if="property.type == 'text'">
										<div v-for="item in property.items" class="form-group row m-1">
											<label class="col-sm-4 col-form-label pl-0 mt-1">{{item.label}}</label>
											<input type="text" v-if="property.perm == 'ro'" readonly class="col-sm-8 form-control mt-1" :value="item.value">
											<input type="text" v-else class="col-sm-8 form-control mt-1" :class="dirty(item)" :value="value(item)" @keyup="newValue(item, $event.target.value)">
										</div>
										<template v-if="property.perm != 'ro'">
											<div class="float-right mt-1 mr-1">
												<button type="submit" class="btn btn-sm btn-primary ml-1" @click.prevent="set(property)">Submit</button>
												<button class="btn btn-sm btn-default ml-1" @click.prevent="reset(property)">Reset</button>
											</div>
										</template>
									</template>
									<template v-else-if="property.type == 'number'">
										<div v-for="item in property.items" class="form-group row m-1">
											<template v-if="property.perm == 'ro'">
												<label class="col-sm-9 col-form-label pl-0 mt-1">{{item.label}}</label>
												<input type="text" readonly class="col-sm-3 form-control mt-1" style="min-width: 5rem" :class="dirty(item)" :value="value(item)" @keyup="newValue(item, parseFloat($event.target.value))">
											</template>
											<template v-else>
												<label class="col-sm-5 col-form-label pl-0 mt-1">{{item.label}}</label>
												<input type="text" readonly class="col-sm-3 form-control mt-1" :value="item.target" style="min-width: 5rem">
												<input type="text" class="col-sm-3 offset-sm-1 form-control mt-1" style="min-width: 5rem" :class="dirty(item)" :value="value(item)" @keyup="newValue(item, parseFloat($event.target.value))">
											</template>
										</div>
										<template v-if="property.perm != 'ro'">
											<div class="float-right mt-1 mr-1">
												<button type="submit" class="btn btn-sm btn-primary ml-1" @click.prevent="set(property)">Submit</button>
												<button class="btn btn-sm btn-default ml-1" @click.prevent="reset(property)">Reset</button>
											</div>
										</template>
									</template>
									<template v-else-if="property.type == 'switch'">
										<div class="form-group row m-0">
											<div v-for="item in property.items" class="col-sm-3 p-0 m-0 pr-2" style="min-width: 15rem">
												<button v-if="item.value && property.rule == 'OneOfMany'" disabled class="btn btn-sm btn-primary w-100 m-1">{{item.label}}</button>
												<button v-else class="btn btn-sm w-100 m-1" :class="item.value ? 'btn-primary' : 'btn-default'" @click.prevent="setSwitch(property, item.name, !item.value)">{{item.label}}</button>
											</div>
										</div>
									</template>
									<template v-else-if="property.type == 'light'">
										<div class="form-group row m-0">
											<div v-for="item in property.items" class="col-sm-3 p-0 m-0 pr-2" style="min-width: 15rem">
												<button disabled class="btn btn-sm w-100 m-1" :class="state(item)">{{item.label}}</button>
											</div>
										</div>
									</template>
									<template v-else>
										<small>{{property}}</small>
									</template>
								</form>
							</div>

						</div>

					</div>
				</div>

			</div>
		</div>
		</div>`
});

Vue.component('indigo-select-multi-item', {
	props: {
		property: Object,
		label: String,
		prefix: String
	},
	methods: {
		items: function() {
			var result = [];
			for (i in this.property.itemsByLabel) {
				var item = this.property.items[i];
				if (item.name.startsWith(this.prefix)) result.push(item);
			}
			return result;
		},
		value: function() {
			var result = null;
			for (i in this.property.itemsByLabel) {
				var item = this.property.items[i];
				if (item.value && item.name.startsWith(this.prefix)) result = result == null ? item.label : result + "; " + item.label;
			}
			return result;
		},
		state: function() {
			return this.property.state.toLowerCase() + "-state";
		},
		change: function(item) {
			var values = {};
			values[item.name] = !item.value;
			changeProperty(this.property.device, this.property.name, values);
		}
	},
	template: `
		<div class="input-group p-1">
		<div class="input-group-prepend">
			<span class="input-group-text" id="inputGroup-sizing-default" style="width: 10em;" :class="state()">{{label}}</span>
		</div>
		<input readonly type="text" class="form-control" :value="value()">
		<div class="input-group-append">
			<button class="btn dropdown-toggle dropdown-toggle-split btn-outline-secondary" type="button" data-toggle="dropdown"></button>
			<div class="dropdown-menu">
				<a class="dropdown-item" :class="item.value ? 'checked' : ''" href="#" v-for="item in items()" @click="change(item)">{{item.label}}</a>
			</div>
		</div>
		</div>`
});